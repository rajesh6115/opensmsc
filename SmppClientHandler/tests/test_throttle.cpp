#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <asio.hpp>
#include <sys/socket.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>

#include "smpp_pdu.hpp"
#include "smpp_session.hpp"
#include "generated/IServer_adaptor.hpp"
#include "generated/IAuth_adaptor.hpp"

// ── Stubs ─────────────────────────────────────────────────────────────────────

struct ThrottleServerStub : public com::telecom::smpp::IServer_adaptor {
    ThrottleServerStub(sdbus::IObject& obj) : IServer_adaptor(obj) {}
    sdbus::UnixFd GetSocket(const std::string&) override { return sdbus::UnixFd{-1}; }
    void UpdateSessionDetails(const std::string&, const std::string&,
                              const std::string&) override {}
    uint32_t GetConnectionCount(const std::string&) override { return 1; }
    std::vector<sdbus::Struct<std::string,std::string,std::string,std::string>>
    GetAllSessions() override { return {}; }
    void SetMaxConnectionsPerIp(const std::string&, const uint32_t&) override {}
    void DisconnectAll(const std::string&, const std::string&) override {}
    std::string RouteMessage(const std::string&, const std::string&,
                             const std::string&, const std::string&) override { return {}; }
};

struct ThrottleAuthStub : public com::telecom::smpp::IAuth_adaptor {
    ThrottleAuthStub(sdbus::IObject& obj) : IAuth_adaptor(obj) {}
    std::tuple<bool,uint32_t> Authenticate(const std::string&, const std::string& sid,
                                            const std::string& pwd,
                                            const std::string&) override {
        if (sid == "esme1" && pwd == "pass1") return {true, 0u};
        return {false, 0x0Eu};
    }
    void ReloadCredentials() override {}
};

// ── Fixture ───────────────────────────────────────────────────────────────────

class ThrottleTest : public ::testing::Test {
protected:
    std::string suffix, server_svc, auth_svc, client_svc;
    std::unique_ptr<sdbus::IConnection> server_conn, auth_conn, session_conn;
    std::unique_ptr<sdbus::IObject>     server_obj, auth_obj, client_obj;
    std::unique_ptr<ThrottleServerStub> server_stub;
    std::unique_ptr<ThrottleAuthStub>   auth_stub;
    std::unique_ptr<SmppSession>        session;
    std::unique_ptr<asio::io_context>   io_ctx;
    int client_fd{-1};
    std::thread server_evt, auth_evt, session_evt, io_thread;

    // max_submit_per_sec controls bucket size — set low for fast tests
    void SetUpWithRate(unsigned rate) {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        suffix     = std::to_string(getpid()) + "r" + std::to_string(ts % 100000);
        server_svc = "com.telecom.smpp.Server.r" + suffix;
        auth_svc   = "com.telecom.smpp.Auth.r"   + suffix;
        client_svc = "com.telecom.smpp.Client.r" + suffix;

        int sv[2];
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
        int session_fd = sv[0]; client_fd = sv[1];

        server_conn = sdbus::createSessionBusConnection(server_svc);
        server_obj  = sdbus::createObject(*server_conn, "/com/telecom/smpp/server");
        server_stub = std::make_unique<ThrottleServerStub>(*server_obj);
        server_obj->finishRegistration();
        server_evt = std::thread([this]{ server_conn->enterEventLoop(); });

        auth_conn = sdbus::createSessionBusConnection(auth_svc);
        auth_obj  = sdbus::createObject(*auth_conn, "/com/telecom/smpp/auth");
        auth_stub = std::make_unique<ThrottleAuthStub>(*auth_obj);
        auth_obj->finishRegistration();
        auth_evt = std::thread([this]{ auth_conn->enterEventLoop(); });

        session_conn = sdbus::createSessionBusConnection(client_svc);
        client_obj   = sdbus::createObject(*session_conn, "/com/telecom/smpp/client");
        io_ctx = std::make_unique<asio::io_context>();

        session = std::make_unique<SmppSession>(
            *io_ctx, session_fd, "uuid-" + suffix, "127.0.0.1",
            *client_obj, *session_conn, server_svc, auth_svc,
            /*el_interval=*/0, /*el_timeout=*/10, rate);
        client_obj->finishRegistration();
        session_evt = std::thread([this]{ session_conn->enterEventLoop(); });
        io_thread = std::thread([this]{ session->start(); io_ctx->run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    void SetUp() override { SetUpWithRate(3); }  // 3 msg/s bucket

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

    void bind_transmitter() {
        std::vector<uint8_t> body;
        for (char c : std::string("esme1")) body.push_back(c); body.push_back(0);
        for (char c : std::string("pass1")) body.push_back(c); body.push_back(0);
        body.push_back(0); body.push_back(0x34); body.push_back(0);
        body.push_back(0); body.push_back(0);
        uint32_t total = 16 + static_cast<uint32_t>(body.size());
        std::vector<uint8_t> pdu(total);
        smpp::write_u32(pdu.data(),    total);
        smpp::write_u32(pdu.data()+4,  smpp::BIND_TRANSMITTER);
        smpp::write_u32(pdu.data()+8,  0);
        smpp::write_u32(pdu.data()+12, 1);
        std::memcpy(pdu.data()+16, body.data(), body.size());
        write_pdu(pdu);
        auto resp = read_pdu();
        ASSERT_GE(resp.size(), 16u);
        ASSERT_EQ(smpp::read_u32(resp.data()+8), smpp::ESME_ROK);
    }

    std::vector<uint8_t> make_submit_sm(uint32_t seq) {
        std::vector<uint8_t> body(17, 0);  // all-zero minimal body
        body[15] = 5;
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

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(ThrottleTest, FirstMessageWithinBucketSucceeds) {
    bind_transmitter();
    write_pdu(make_submit_sm(2));
    auto resp = read_pdu();
    ASSERT_GE(resp.size(), 16u);
    EXPECT_EQ(smpp::read_u32(resp.data()+4), smpp::SUBMIT_SM_RESP);
    EXPECT_EQ(smpp::read_u32(resp.data()+8), smpp::ESME_ROK);
}

TEST_F(ThrottleTest, ExceedingBucketReturnsThrottled) {
    bind_transmitter();

    // Drain the 3-token bucket immediately
    uint32_t throttled_count = 0;
    for (uint32_t i = 0; i < 10; ++i) {
        write_pdu(make_submit_sm(i + 2));
        auto resp = read_pdu();
        ASSERT_GE(resp.size(), 16u);
        EXPECT_EQ(smpp::read_u32(resp.data()+4), smpp::SUBMIT_SM_RESP);
        if (smpp::read_u32(resp.data()+8) == smpp::ESME_RTHROTTLED)
            ++throttled_count;
    }
    // With a 3 msg/s bucket and 10 rapid messages, at least some must be throttled
    EXPECT_GT(throttled_count, 0u) << "expected at least one ESME_RTHROTTLED";
    EXPECT_LT(throttled_count, 10u) << "first few messages should pass";
}

// Separate fixture with unlimited rate (rate=0)
class ThrottleUnlimitedTest : public ThrottleTest {
    void SetUp() override { SetUpWithRate(0); }
};

TEST_F(ThrottleUnlimitedTest, UnlimitedRateNeverThrottles) {
    bind_transmitter();
    for (uint32_t i = 0; i < 20; ++i) {
        write_pdu(make_submit_sm(i + 2));
        auto resp = read_pdu();
        ASSERT_GE(resp.size(), 16u);
        EXPECT_EQ(smpp::read_u32(resp.data()+8), smpp::ESME_ROK)
            << "message " << i << " should not be throttled";
    }
}

TEST_F(ThrottleTest, BucketRefillsAfterDelay) {
    bind_transmitter();

    // Drain the bucket
    for (uint32_t i = 0; i < 10; ++i) {
        write_pdu(make_submit_sm(i + 2));
        read_pdu();
    }

    // Wait for 1 full second — should refill at least 3 tokens
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // Next messages should succeed again
    uint32_t ok_count = 0;
    for (uint32_t i = 0; i < 3; ++i) {
        write_pdu(make_submit_sm(100 + i));
        auto resp = read_pdu();
        ASSERT_GE(resp.size(), 16u);
        if (smpp::read_u32(resp.data()+8) == smpp::ESME_ROK) ++ok_count;
    }
    EXPECT_GT(ok_count, 0u) << "bucket should have refilled after 1s";
}
