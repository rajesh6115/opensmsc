#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <asio.hpp>
#include <sys/socket.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>

#include "smpp_pdu.hpp"
#include "smpp_session.hpp"
#include "generated/IServer_adaptor.hpp"
#include "generated/IAuth_adaptor.hpp"

// ── Stubs ─────────────────────────────────────────────────────────────────────

struct IntegServerStub : public com::telecom::smpp::IServer_adaptor {
    std::atomic<int> update_count{0};
    std::string last_state;

    IntegServerStub(sdbus::IObject& obj) : IServer_adaptor(obj) {}
    sdbus::UnixFd GetSocket(const std::string&) override { return sdbus::UnixFd{-1}; }
    void UpdateSessionDetails(const std::string&, const std::string&,
                              const std::string& state) override {
        last_state = state;
        ++update_count;
    }
    uint32_t GetConnectionCount(const std::string&) override { return 1; }
    std::vector<sdbus::Struct<std::string,std::string,std::string,std::string>>
    GetAllSessions() override { return {}; }
    void SetMaxConnectionsPerIp(const std::string&, const uint32_t&) override {}
    void DisconnectAll(const std::string&, const std::string&) override {}
    std::string RouteMessage(const std::string&, const std::string&,
                             const std::string&, const std::string&) override { return {}; }
};

struct IntegAuthStub : public com::telecom::smpp::IAuth_adaptor {
    IntegAuthStub(sdbus::IObject& obj) : IAuth_adaptor(obj) {}
    std::tuple<bool,uint32_t> Authenticate(const std::string&, const std::string& sid,
                                            const std::string& pwd,
                                            const std::string&) override {
        if (sid == "esme1" && pwd == "secret1") return {true, 0u};
        return {false, 0x0Eu};
    }
    void ReloadCredentials() override {}
};

// ── Fixture ───────────────────────────────────────────────────────────────────

class SmppIntegrationTest : public ::testing::Test {
protected:
    std::string suffix, server_svc, auth_svc, client_svc;
    std::unique_ptr<sdbus::IConnection> server_conn, auth_conn, session_conn;
    std::unique_ptr<sdbus::IObject>     server_obj, auth_obj, client_obj;
    std::unique_ptr<IntegServerStub>    server_stub;
    std::unique_ptr<IntegAuthStub>      auth_stub;
    std::unique_ptr<SmppSession>        session;
    std::unique_ptr<asio::io_context>   io_ctx;
    int client_fd{-1};
    std::thread server_evt, auth_evt, session_evt, io_thread;

    void SetUp() override {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        suffix     = std::to_string(getpid()) + "i" + std::to_string(ts % 100000);
        server_svc = "com.telecom.smpp.Server.i" + suffix;
        auth_svc   = "com.telecom.smpp.Auth.i"   + suffix;
        client_svc = "com.telecom.smpp.Client.i" + suffix;

        int sv[2];
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
        int session_fd = sv[0];
        client_fd      = sv[1];

        server_conn = sdbus::createSessionBusConnection(server_svc);
        server_obj  = sdbus::createObject(*server_conn, "/com/telecom/smpp/server");
        server_stub = std::make_unique<IntegServerStub>(*server_obj);
        server_obj->finishRegistration();
        server_evt = std::thread([this]{ server_conn->enterEventLoop(); });

        auth_conn = sdbus::createSessionBusConnection(auth_svc);
        auth_obj  = sdbus::createObject(*auth_conn, "/com/telecom/smpp/auth");
        auth_stub = std::make_unique<IntegAuthStub>(*auth_obj);
        auth_obj->finishRegistration();
        auth_evt = std::thread([this]{ auth_conn->enterEventLoop(); });

        session_conn = sdbus::createSessionBusConnection(client_svc);
        client_obj   = sdbus::createObject(*session_conn, "/com/telecom/smpp/client");
        io_ctx = std::make_unique<asio::io_context>();

        // Disable keepalive (interval=0) so the integration test controls timing
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

    std::vector<uint8_t> make_bind_tx(const std::string& sid, const std::string& pwd,
                                       uint32_t seq = 1) {
        std::vector<uint8_t> body;
        for (char c : sid) body.push_back(c); body.push_back(0);
        for (char c : pwd) body.push_back(c); body.push_back(0);
        body.push_back(0);      // system_type
        body.push_back(0x34);   // interface_version
        body.push_back(0);      // addr_ton
        body.push_back(0);      // addr_npi
        body.push_back(0);      // address_range
        uint32_t total = 16 + static_cast<uint32_t>(body.size());
        std::vector<uint8_t> pdu(total);
        smpp::write_u32(pdu.data(),    total);
        smpp::write_u32(pdu.data()+4,  smpp::BIND_TRANSMITTER);
        smpp::write_u32(pdu.data()+8,  0);
        smpp::write_u32(pdu.data()+12, seq);
        std::memcpy(pdu.data()+16, body.data(), body.size());
        return pdu;
    }

    std::vector<uint8_t> make_submit_sm(uint32_t seq = 2) {
        // Minimal submit_sm: all CStr fields empty, no UDH
        std::vector<uint8_t> body;
        // service_type, src_ton, src_npi, src_addr, dst_ton, dst_npi, dst_addr
        body.push_back(0);  // service_type ""
        body.push_back(0);  // src_ton
        body.push_back(0);  // src_npi
        body.push_back(0);  // src_addr ""
        body.push_back(0);  // dst_ton
        body.push_back(0);  // dst_npi
        body.push_back(0);  // dst_addr ""
        // esm_class, protocol_id, priority_flag
        body.push_back(0); body.push_back(0); body.push_back(0);
        // schedule_delivery_time "", validity_period ""
        body.push_back(0); body.push_back(0);
        // registered_delivery, replace_if_present, data_coding, sm_default_msg_id
        body.push_back(0); body.push_back(0); body.push_back(0); body.push_back(0);
        // sm_length = 5, short_message = "hello"
        body.push_back(5);
        for (char c : std::string("hello")) body.push_back(static_cast<uint8_t>(c));

        uint32_t total = 16 + static_cast<uint32_t>(body.size());
        std::vector<uint8_t> pdu(total);
        smpp::write_u32(pdu.data(),    total);
        smpp::write_u32(pdu.data()+4,  smpp::SUBMIT_SM);
        smpp::write_u32(pdu.data()+8,  0);
        smpp::write_u32(pdu.data()+12, seq);
        std::memcpy(pdu.data()+16, body.data(), body.size());
        return pdu;
    }
};

// ── Integration test: full session lifecycle ───────────────────────────────────

TEST_F(SmppIntegrationTest, FullSessionLifecycle) {
    // 1. Bind as transmitter
    write_pdu(make_bind_tx("esme1", "secret1", 1));
    auto bind_resp = read_pdu();
    ASSERT_GE(bind_resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(bind_resp.data()+4), smpp::BIND_TRANSMITTER_RESP);
    EXPECT_EQ(smpp::read_u32(bind_resp.data()+8), smpp::ESME_ROK);
    EXPECT_EQ(smpp::read_u32(bind_resp.data()+12), 1u);

    // 2. Server should have updated session details with BOUND_TX state
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(server_stub->update_count.load(), 1);
    EXPECT_EQ(server_stub->last_state, "BOUND_TX");

    // 3. Submit a short message — should succeed (ESME_ROK) since BOUND_TX
    write_pdu(make_submit_sm(2));
    auto submit_resp = read_pdu();
    ASSERT_GE(submit_resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(submit_resp.data()+4), smpp::SUBMIT_SM_RESP);
    EXPECT_EQ(smpp::read_u32(submit_resp.data()+8), smpp::ESME_ROK);
    EXPECT_EQ(smpp::read_u32(submit_resp.data()+12), 2u);

    // 4. Unbind gracefully
    std::vector<uint8_t> unbind(16, 0);
    smpp::write_u32(unbind.data(),    16);
    smpp::write_u32(unbind.data()+4,  smpp::UNBIND);
    smpp::write_u32(unbind.data()+8,  0);
    smpp::write_u32(unbind.data()+12, 3);
    write_pdu(unbind);

    auto unbind_resp = read_pdu();
    ASSERT_GE(unbind_resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(unbind_resp.data()+4), smpp::UNBIND_RESP);
    EXPECT_EQ(smpp::read_u32(unbind_resp.data()+8), smpp::ESME_ROK);

    // 5. After unbind the server closes the socket — a subsequent read returns EOF
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    char buf[1];
    struct timeval tv{1, 0};
    ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int r = ::recv(client_fd, buf, 1, 0);
    EXPECT_LE(r, 0);
}

TEST_F(SmppIntegrationTest, SubmitSmBeforeBindIsRejected) {
    // submit_sm before binding must get ESME_RINVBNDSTS
    write_pdu(make_submit_sm(1));
    auto resp = read_pdu();
    ASSERT_GE(resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(resp.data()+4), smpp::SUBMIT_SM_RESP);
    EXPECT_NE(smpp::read_u32(resp.data()+8), smpp::ESME_ROK);
}

TEST_F(SmppIntegrationTest, SubmitSmRespContainsMessageId) {
    // Bind first
    write_pdu(make_bind_tx("esme1", "secret1", 1));
    auto bind_resp = read_pdu();
    ASSERT_EQ(smpp::read_u32(bind_resp.data()+8), smpp::ESME_ROK);

    // Submit SM — response body must contain a non-empty message_id C-string
    write_pdu(make_submit_sm(2));
    auto resp = read_pdu();
    ASSERT_GE(resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(resp.data()+4), smpp::SUBMIT_SM_RESP);
    EXPECT_EQ(smpp::read_u32(resp.data()+8), smpp::ESME_ROK);

    // Body (offset 16) must be a non-empty null-terminated message_id
    ASSERT_GT(resp.size(), 17u) << "submit_sm_resp body (message_id) is missing";
    size_t off = 16;
    std::string msg_id = smpp::read_cstr(resp, off);
    EXPECT_FALSE(msg_id.empty()) << "message_id must be non-empty";

    // A second submit should produce a different message_id (uniqueness)
    write_pdu(make_submit_sm(3));
    auto resp2 = read_pdu();
    ASSERT_GT(resp2.size(), 17u);
    size_t off2 = 16;
    std::string msg_id2 = smpp::read_cstr(resp2, off2);
    EXPECT_NE(msg_id, msg_id2) << "consecutive message_ids must be unique";
}
