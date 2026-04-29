#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <asio.hpp>
#include <sys/socket.h>
#include <thread>
#include <chrono>
#include <cstring>

#include "smpp_pdu.hpp"
#include "smpp_session.hpp"
#include "generated/IClientHandler_proxy.hpp"
#include "generated/IServer_adaptor.hpp"
#include "generated/IAuth_adaptor.hpp"

// ── Stubs ─────────────────────────────────────────────────────────────────────

struct DlvrServerStub : public com::telecom::smpp::IServer_adaptor {
    DlvrServerStub(sdbus::IObject& obj) : IServer_adaptor(obj) {}
    sdbus::UnixFd GetSocket(const std::string&) override { return sdbus::UnixFd{-1}; }
    void UpdateSessionDetails(const std::string&, const std::string&,
                              const std::string&) override {}
    uint32_t GetConnectionCount(const std::string&) override { return 1; }
    std::vector<sdbus::Struct<std::string,std::string,std::string,std::string>>
    GetAllSessions() override { return {}; }
    void SetMaxConnectionsPerIp(const std::string&, const uint32_t&) override {}
    void DisconnectAll(const std::string&, const std::string&) override {}
};

struct DlvrAuthStub : public com::telecom::smpp::IAuth_adaptor {
    DlvrAuthStub(sdbus::IObject& obj) : IAuth_adaptor(obj) {}
    std::tuple<bool,uint32_t> Authenticate(const std::string&, const std::string& sid,
                                            const std::string& pwd,
                                            const std::string&) override {
        if (sid == "esme1" && pwd == "pass1") return {true, 0u};
        return {false, 0x0Eu};
    }
    void ReloadCredentials() override {}
};

// ── Fixture ───────────────────────────────────────────────────────────────────

class DeliverSmTest : public ::testing::Test {
protected:
    std::string suffix, server_svc, auth_svc, client_svc;
    std::unique_ptr<sdbus::IConnection> server_conn, auth_conn, session_conn;
    std::unique_ptr<sdbus::IObject>     server_obj, auth_obj, client_obj;
    std::unique_ptr<DlvrServerStub>     server_stub;
    std::unique_ptr<DlvrAuthStub>       auth_stub;
    std::unique_ptr<SmppSession>        session;
    std::unique_ptr<asio::io_context>   io_ctx;
    int client_fd{-1};
    std::thread server_evt, auth_evt, session_evt, io_thread;

    void SetUp() override {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        suffix     = std::to_string(getpid()) + "d" + std::to_string(ts % 100000);
        server_svc = "com.telecom.smpp.Server.d" + suffix;
        auth_svc   = "com.telecom.smpp.Auth.d"   + suffix;
        client_svc = "com.telecom.smpp.Client.d" + suffix;

        int sv[2];
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
        int session_fd = sv[0]; client_fd = sv[1];

        server_conn = sdbus::createSessionBusConnection(server_svc);
        server_obj  = sdbus::createObject(*server_conn, "/com/telecom/smpp/server");
        server_stub = std::make_unique<DlvrServerStub>(*server_obj);
        server_obj->finishRegistration();
        server_evt = std::thread([this]{ server_conn->enterEventLoop(); });

        auth_conn = sdbus::createSessionBusConnection(auth_svc);
        auth_obj  = sdbus::createObject(*auth_conn, "/com/telecom/smpp/auth");
        auth_stub = std::make_unique<DlvrAuthStub>(*auth_obj);
        auth_obj->finishRegistration();
        auth_evt = std::thread([this]{ auth_conn->enterEventLoop(); });

        session_conn = sdbus::createSessionBusConnection(client_svc);
        client_obj   = sdbus::createObject(*session_conn, "/com/telecom/smpp/client");
        io_ctx = std::make_unique<asio::io_context>();

        session = std::make_unique<SmppSession>(
            *io_ctx, session_fd, "uuid-" + suffix, "127.0.0.1",
            *client_obj, *session_conn, server_svc, auth_svc, 0);
        client_obj->finishRegistration();
        session_evt = std::thread([this]{ session_conn->enterEventLoop(); });

        io_thread = std::thread([this]{ session->start(); io_ctx->run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    void TearDown() override {
        io_ctx->stop();
        if (io_thread.joinable())   io_thread.join();
        server_conn->leaveEventLoop();
        auth_conn->leaveEventLoop();
        session_conn->leaveEventLoop();
        if (server_evt.joinable())  server_evt.join();
        if (auth_evt.joinable())    auth_evt.join();
        if (session_evt.joinable()) session_evt.join();
        if (client_fd >= 0) ::close(client_fd);
    }

    void write_pdu(const std::vector<uint8_t>& pdu) {
        ::send(client_fd, pdu.data(), pdu.size(), MSG_NOSIGNAL);
    }

    std::vector<uint8_t> read_pdu(int timeout_ms = 2000) {
        struct timeval tv{timeout_ms/1000, (timeout_ms%1000)*1000};
        ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        uint8_t hdr[16];
        if (::recv(client_fd, hdr, 16, MSG_WAITALL) != 16) return {};
        uint32_t len = smpp::read_u32(hdr);
        std::vector<uint8_t> buf(len);
        std::memcpy(buf.data(), hdr, 16);
        if (len > 16) ::recv(client_fd, buf.data()+16, len-16, MSG_WAITALL);
        return buf;
    }

    // Bind as BOUND_RX
    void bind_receiver() {
        std::vector<uint8_t> body;
        for (char c : std::string("esme1")) body.push_back(c); body.push_back(0);
        for (char c : std::string("pass1")) body.push_back(c); body.push_back(0);
        body.push_back(0); body.push_back(0x34);
        body.push_back(0); body.push_back(0); body.push_back(0);
        uint32_t total = 16 + static_cast<uint32_t>(body.size());
        std::vector<uint8_t> pdu(total);
        smpp::write_u32(pdu.data(),    total);
        smpp::write_u32(pdu.data()+4,  smpp::BIND_RECEIVER);
        smpp::write_u32(pdu.data()+8,  0);
        smpp::write_u32(pdu.data()+12, 1);
        std::memcpy(pdu.data()+16, body.data(), body.size());
        write_pdu(pdu);
        auto resp = read_pdu();
        ASSERT_GE(resp.size(), 16u);
        ASSERT_EQ(smpp::read_u32(resp.data()+4), smpp::BIND_RECEIVER_RESP);
        ASSERT_EQ(smpp::read_u32(resp.data()+8), smpp::ESME_ROK);
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(DeliverSmTest, DeliverSmSentToReceiver) {
    bind_receiver();

    // Call DeliverSm via D-Bus proxy (simulates SMSC core routing an MO message)
    auto proxy = sdbus::createProxy(*session_conn, client_svc, "/com/telecom/smpp/client");
    uint32_t seq_out = 0;
    proxy->callMethod("DeliverSm")
          .onInterface("com.telecom.smpp.IClientHandler")
          .withArguments(std::string("441234567890"),
                         std::string("esme1"),
                         std::string("Hello ESME"))
          .storeResultsTo(seq_out);
    EXPECT_NE(seq_out, 0u);

    // ESME side should receive a deliver_sm PDU
    auto pdu = read_pdu();
    ASSERT_GE(pdu.size(), 16u);
    EXPECT_EQ(smpp::read_u32(pdu.data()+4), smpp::DELIVER_SM);
    EXPECT_EQ(smpp::read_u32(pdu.data()+8), smpp::ESME_ROK);
    EXPECT_EQ(smpp::read_u32(pdu.data()+12), seq_out);

    // ESME sends back deliver_sm_resp — session must not disconnect
    auto resp = smpp::make_response(smpp::DELIVER_SM_RESP, smpp::ESME_ROK, seq_out);
    write_pdu(resp);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Session still alive: send enquire_link, expect echo
    auto el = smpp::make_response(smpp::ENQUIRE_LINK, 0, 99);
    write_pdu(el);
    auto el_resp = read_pdu();
    ASSERT_GE(el_resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(el_resp.data()+4), smpp::ENQUIRE_LINK_RESP);
}

TEST_F(DeliverSmTest, DeliverSmRejectedWhenNotReceiver) {
    // Session is OPEN (not bound) — DeliverSm must throw
    auto proxy = sdbus::createProxy(*session_conn, client_svc, "/com/telecom/smpp/client");
    EXPECT_THROW(
        proxy->callMethod("DeliverSm")
              .onInterface("com.telecom.smpp.IClientHandler")
              .withArguments(std::string("src"), std::string("dst"), std::string("msg")),
        sdbus::Error
    );
}

TEST_F(DeliverSmTest, DeliverSmBodyContainsSrcAndDst) {
    bind_receiver();

    auto proxy = sdbus::createProxy(*session_conn, client_svc, "/com/telecom/smpp/client");
    uint32_t seq_out = 0;
    proxy->callMethod("DeliverSm")
          .onInterface("com.telecom.smpp.IClientHandler")
          .withArguments(std::string("SRC"), std::string("DST"), std::string("Hi"))
          .storeResultsTo(seq_out);

    auto pdu = read_pdu();
    ASSERT_GE(pdu.size(), 16u);
    EXPECT_EQ(smpp::read_u32(pdu.data()+4), smpp::DELIVER_SM);

    // Decode body: skip service_type(1B), src_ton(1), src_npi(1), then src_addr cstr
    size_t off = 16;
    off++; // service_type null
    off++; // src_ton
    off++; // src_npi
    std::string src = smpp::read_cstr(pdu, off);
    off++; // dst_ton
    off++; // dst_npi
    std::string dst = smpp::read_cstr(pdu, off);
    EXPECT_EQ(src, "SRC");
    EXPECT_EQ(dst, "DST");
}
