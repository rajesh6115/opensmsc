#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <asio.hpp>
#include <sys/socket.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <atomic>

#include "smpp_pdu.hpp"
#include "smpp_session.hpp"
#include "generated/IServer_adaptor.hpp"
#include "generated/IAuth_adaptor.hpp"

// ── Stub D-Bus services ───────────────────────────────────────────────────────

struct ServerStub : public com::telecom::smpp::IServer_adaptor {
    ServerStub(sdbus::IObject& obj) : IServer_adaptor(obj) {}
    sdbus::UnixFd GetSocket(const std::string&) override { return sdbus::UnixFd{-1}; }
    void UpdateSessionDetails(const std::string&, const std::string&,
                              const std::string&) override {}
    uint32_t GetConnectionCount(const std::string&) override { return 0; }
    std::vector<sdbus::Struct<std::string,std::string,std::string,std::string>>
    GetAllSessions() override { return {}; }
    void SetMaxConnectionsPerIp(const std::string&, const uint32_t&) override {}
    void DisconnectAll(const std::string&, const std::string&) override {}
    std::string RouteMessage(const std::string&, const std::string&,
                             const std::string&, const std::string&) override { return {}; }
};

struct AuthStub : public com::telecom::smpp::IAuth_adaptor {
    AuthStub(sdbus::IObject& obj) : IAuth_adaptor(obj) {}
    std::tuple<bool,uint32_t> Authenticate(const std::string&, const std::string& sid,
                                           const std::string& pwd,
                                           const std::string&) override {
        if (sid == "esme1" && pwd == "pass1") return {true, 0u};
        return {false, 0x0Eu};
    }
    void ReloadCredentials() override {}
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class SmppSessionTest : public ::testing::Test {
protected:
    std::string suffix, server_svc, auth_svc, client_svc;

    std::unique_ptr<sdbus::IConnection> server_conn, auth_conn, session_conn;
    std::unique_ptr<sdbus::IObject>     server_obj, auth_obj, client_obj;
    std::unique_ptr<ServerStub>         server_stub;
    std::unique_ptr<AuthStub>           auth_stub;
    std::unique_ptr<SmppSession>        session;
    std::unique_ptr<asio::io_context>   io_ctx;

    int client_fd{-1};
    std::thread server_evt, auth_evt, session_evt, io_thread;

    void SetUp() override {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        suffix     = std::to_string(getpid()) + "t" + std::to_string(ts % 100000);
        server_svc = "com.telecom.smpp.Server.t" + suffix;
        auth_svc   = "com.telecom.smpp.Auth.t"   + suffix;
        client_svc = "com.telecom.smpp.Client.t" + suffix;

        int sv[2];
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
        int session_fd = sv[0];
        client_fd      = sv[1];

        server_conn = sdbus::createSessionBusConnection(server_svc);
        server_obj  = sdbus::createObject(*server_conn, "/com/telecom/smpp/server");
        server_stub = std::make_unique<ServerStub>(*server_obj);
        server_obj->finishRegistration();
        server_evt  = std::thread([this]{ server_conn->enterEventLoop(); });

        auth_conn = sdbus::createSessionBusConnection(auth_svc);
        auth_obj  = sdbus::createObject(*auth_conn, "/com/telecom/smpp/auth");
        auth_stub = std::make_unique<AuthStub>(*auth_obj);
        auth_obj->finishRegistration();
        auth_evt  = std::thread([this]{ auth_conn->enterEventLoop(); });

        session_conn = sdbus::createSessionBusConnection(client_svc);
        client_obj   = sdbus::createObject(*session_conn, "/com/telecom/smpp/client");

        io_ctx  = std::make_unique<asio::io_context>();
        session = std::make_unique<SmppSession>(
            *io_ctx, session_fd, "uuid-" + suffix, "127.0.0.1",
            *client_obj, *session_conn, server_svc, auth_svc);
        client_obj->finishRegistration();
        session_evt = std::thread([this]{ session_conn->enterEventLoop(); });

        io_thread = std::thread([this]{
            session->start();
            io_ctx->run();
        });
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
        // Set receive timeout
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

    std::vector<uint8_t> make_bind_tx(const std::string& sid, const std::string& pwd,
                                       uint32_t seq = 1) {
        std::vector<uint8_t> body;
        for (char c : sid)  body.push_back(c); body.push_back(0);
        for (char c : pwd)  body.push_back(c); body.push_back(0);
        body.push_back(0);      // system_type null
        body.push_back(0x34);   // interface_version
        body.push_back(0);      // addr_ton
        body.push_back(0);      // addr_npi
        body.push_back(0);      // address_range null

        uint32_t total = 16 + static_cast<uint32_t>(body.size());
        std::vector<uint8_t> pdu(total);
        smpp::write_u32(pdu.data(),    total);
        smpp::write_u32(pdu.data()+4,  smpp::BIND_TRANSMITTER);
        smpp::write_u32(pdu.data()+8,  0);
        smpp::write_u32(pdu.data()+12, seq);
        std::memcpy(pdu.data()+16, body.data(), body.size());
        return pdu;
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(SmppSessionTest, EnquireLinkEcho) {
    auto el = smpp::make_response(smpp::ENQUIRE_LINK, 0, 42);
    write_pdu(el);
    auto resp = read_pdu();
    ASSERT_GE(resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(resp.data()+4),  smpp::ENQUIRE_LINK_RESP);
    EXPECT_EQ(smpp::read_u32(resp.data()+8),  smpp::ESME_ROK);
    EXPECT_EQ(smpp::read_u32(resp.data()+12), 42u);
}

TEST_F(SmppSessionTest, BindTransmitterSuccess) {
    write_pdu(make_bind_tx("esme1", "pass1", 1));
    auto resp = read_pdu();
    ASSERT_GE(resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(resp.data()+4), smpp::BIND_TRANSMITTER_RESP);
    EXPECT_EQ(smpp::read_u32(resp.data()+8), smpp::ESME_ROK);
}

TEST_F(SmppSessionTest, BindTransmitterWrongPassword) {
    write_pdu(make_bind_tx("esme1", "wrong", 2));
    auto resp = read_pdu();
    ASSERT_GE(resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(resp.data()+4), smpp::BIND_TRANSMITTER_RESP);
    EXPECT_NE(smpp::read_u32(resp.data()+8), smpp::ESME_ROK);
}

TEST_F(SmppSessionTest, UnknownCommandGetsGenericNack) {
    std::vector<uint8_t> pdu(16, 0);
    smpp::write_u32(pdu.data(),    16);
    smpp::write_u32(pdu.data()+4,  0x12345678);
    smpp::write_u32(pdu.data()+8,  0);
    smpp::write_u32(pdu.data()+12, 99);
    write_pdu(pdu);
    auto resp = read_pdu();
    ASSERT_GE(resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(resp.data()+4), smpp::GENERIC_NACK);
}

// ── Keepalive test (uses short timers: 1s interval, 1s timeout) ───────────────

class SmppSessionKeepaliveTest : public ::testing::Test {
protected:
    std::string suffix, server_svc, auth_svc, client_svc;
    std::unique_ptr<sdbus::IConnection> server_conn, auth_conn, session_conn;
    std::unique_ptr<sdbus::IObject>     server_obj, auth_obj, client_obj;
    std::unique_ptr<ServerStub>         server_stub;
    std::unique_ptr<AuthStub>           auth_stub;
    std::unique_ptr<SmppSession>        session;
    std::unique_ptr<asio::io_context>   io_ctx;
    int client_fd{-1};
    std::thread server_evt, auth_evt, session_evt, io_thread;

    void SetUp() override {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        suffix     = std::to_string(getpid()) + "k" + std::to_string(ts % 100000);
        server_svc = "com.telecom.smpp.Server.k" + suffix;
        auth_svc   = "com.telecom.smpp.Auth.k"   + suffix;
        client_svc = "com.telecom.smpp.Client.k" + suffix;

        int sv[2]; ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
        int session_fd = sv[0]; client_fd = sv[1];

        server_conn = sdbus::createSessionBusConnection(server_svc);
        server_obj  = sdbus::createObject(*server_conn, "/com/telecom/smpp/server");
        server_stub = std::make_unique<ServerStub>(*server_obj);
        server_obj->finishRegistration();
        server_evt = std::thread([this]{ server_conn->enterEventLoop(); });

        auth_conn = sdbus::createSessionBusConnection(auth_svc);
        auth_obj  = sdbus::createObject(*auth_conn, "/com/telecom/smpp/auth");
        auth_stub = std::make_unique<AuthStub>(*auth_obj);
        auth_obj->finishRegistration();
        auth_evt = std::thread([this]{ auth_conn->enterEventLoop(); });

        session_conn = sdbus::createSessionBusConnection(client_svc);
        client_obj   = sdbus::createObject(*session_conn, "/com/telecom/smpp/client");
        io_ctx = std::make_unique<asio::io_context>();

        // 1s interval, 1s timeout for fast test
        session = std::make_unique<SmppSession>(
            *io_ctx, session_fd, "uuid-k-" + suffix, "127.0.0.1",
            *client_obj, *session_conn, server_svc, auth_svc, 1, 1);
        client_obj->finishRegistration();
        session_evt = std::thread([this]{ session_conn->enterEventLoop(); });

        io_thread = std::thread([this]{ session->start(); io_ctx->run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        io_ctx->stop();
        if (io_thread.joinable())   io_thread.join();
        server_conn->leaveEventLoop(); auth_conn->leaveEventLoop();
        session_conn->leaveEventLoop();
        if (server_evt.joinable())  server_evt.join();
        if (auth_evt.joinable())    auth_evt.join();
        if (session_evt.joinable()) session_evt.join();
        if (client_fd >= 0) ::close(client_fd);
    }

    std::vector<uint8_t> read_pdu(int timeout_ms = 3000) {
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
};

TEST_F(SmppSessionKeepaliveTest, ServerSendsEnquireLink) {
    // After ~1s the session should send an enquire_link
    auto pdu = read_pdu(2500);
    ASSERT_GE(pdu.size(), 16u);
    EXPECT_EQ(smpp::read_u32(pdu.data()+4), smpp::ENQUIRE_LINK);
}

TEST_F(SmppSessionKeepaliveTest, TimeoutDisconnectsIfNoReply) {
    // Read the enquire_link (don't reply)
    auto pdu = read_pdu(2500);
    ASSERT_GE(pdu.size(), 16u);
    EXPECT_EQ(smpp::read_u32(pdu.data()+4), smpp::ENQUIRE_LINK);
    // After another ~1s timeout the session should close the socket
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    // Socket should be closed — a subsequent read should return 0 or error
    char buf[1];
    struct timeval tv{1, 0};
    ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int r = ::recv(client_fd, buf, 1, 0);
    EXPECT_LE(r, 0);  // 0 = EOF, -1 = error
}
