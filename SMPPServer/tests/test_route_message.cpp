#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <sys/socket.h>

#include "connection_registry.hpp"
#include "smpp_server_service.hpp"
#include "generated/IServer_proxy.hpp"

// We need IClientHandler adaptor to create a receiver stub
// It lives in SmppClientHandler/include — included via CMake include path

static constexpr const char* ISERVER_PATH  = "/com/telecom/smpp/server";
static constexpr const char* ISERVER_IFACE = "com.telecom.smpp.IServer";

// ── Minimal IClientHandler stub for the receiver ─────────────────────────────
// We use sdbus directly (not the generated adaptor) to avoid a cross-module dep.
// The stub registers DeliverSm as a D-Bus method on the session bus.

struct ReceiverStub {
    std::unique_ptr<sdbus::IConnection> conn;
    std::unique_ptr<sdbus::IObject>     obj;
    std::thread                         evt;
    std::atomic<int>                    deliver_count{0};
    std::string                         last_src;

    explicit ReceiverStub(const std::string& svc) {
        conn = sdbus::createSessionBusConnection(svc);
        obj  = sdbus::createObject(*conn, "/com/telecom/smpp/client");

        obj->registerMethod("DeliverSm")
            .onInterface("com.telecom.smpp.IClientHandler")
            .withInputParamNames("src_addr", "dst_addr", "short_message")
            .withOutputParamNames("sequence_num")
            .implementedAs([this](const std::string& src, const std::string&,
                                  const std::string&) -> uint32_t {
                last_src = src;
                ++deliver_count;
                return 0x20000001u;
            });
        obj->finishRegistration();
        evt = std::thread([this]{ conn->enterEventLoop(); });
    }

    ~ReceiverStub() {
        conn->leaveEventLoop();
        if (evt.joinable()) evt.join();
    }
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class RouteMessageTest : public ::testing::Test {
protected:
    std::string suffix, server_svc;
    std::unique_ptr<sdbus::IConnection>  server_conn;
    std::unique_ptr<sdbus::IObject>      server_obj;
    std::shared_ptr<ConnectionRegistry>  registry;
    std::unique_ptr<SmppServerService>   svc;
    std::thread                          server_evt;

    void SetUp() override {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        suffix     = std::to_string(getpid()) + "m" + std::to_string(ts % 100000);
        server_svc = "com.telecom.smpp.Server.m" + suffix;

        server_conn = sdbus::createSessionBusConnection(server_svc);
        server_obj  = sdbus::createObject(*server_conn, ISERVER_PATH);
        registry    = std::make_shared<ConnectionRegistry>(10);
        svc         = std::make_unique<SmppServerService>(*server_obj, *server_conn, registry);
        server_obj->finishRegistration();
        server_evt  = std::thread([this]{ server_conn->enterEventLoop(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        server_conn->leaveEventLoop();
        if (server_evt.joinable()) server_evt.join();
    }

    // Inject a fake session entry directly (bypasses socket/TCP flow)
    void register_fake_session(const std::string& uuid, const std::string& state,
                                const std::string& ip = "127.0.0.1") {
        int sv[2]; ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
        svc->register_session(uuid, sv[0], ip);
        ::close(sv[1]);
        // Advance state via UpdateSessionDetails
        auto proxy = sdbus::createProxy(*server_conn, server_svc, ISERVER_PATH);
        proxy->callMethod("UpdateSessionDetails").onInterface(ISERVER_IFACE)
              .withArguments(uuid, std::string("esme1"), state);
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(RouteMessageTest, RouteMessageDeliversToReceiver) {
    // Register a BOUND_RX receiver whose D-Bus service is com.telecom.smpp.Client.<uuid>
    const std::string rx_uuid = "rx-" + suffix;
    const std::string rx_svc  = "com.telecom.smpp.Client." + rx_uuid;

    ReceiverStub receiver(rx_svc);
    register_fake_session(rx_uuid, "BOUND_RX");

    // Call RouteMessage on the server — should dispatch to receiver's DeliverSm
    auto client = sdbus::createProxy(*server_conn, server_svc, ISERVER_PATH);
    std::string routed_to;
    client->callMethod("RouteMessage").onInterface(ISERVER_IFACE)
          .withArguments(std::string("441234"), std::string("esme1"),
                         std::string("Hello"), std::string("msg-001"))
          .storeResultsTo(routed_to);

    EXPECT_EQ(routed_to, rx_uuid);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(receiver.deliver_count.load(), 1);
}

TEST_F(RouteMessageTest, RouteMessageReturnsEmptyWhenNoReceiver) {
    // No BOUND_RX/TRX sessions registered
    auto client = sdbus::createProxy(*server_conn, server_svc, ISERVER_PATH);
    std::string routed_to;
    client->callMethod("RouteMessage").onInterface(ISERVER_IFACE)
          .withArguments(std::string("src"), std::string("dst"),
                         std::string("msg"), std::string("id-x"))
          .storeResultsTo(routed_to);
    EXPECT_TRUE(routed_to.empty());
}

TEST_F(RouteMessageTest, RouteMessageSkipsBoundTxSessions) {
    // BOUND_TX session must not receive deliver_sm
    const std::string tx_uuid = "tx-" + suffix;
    const std::string tx_svc  = "com.telecom.smpp.Client." + tx_uuid;
    // Don't create a stub — if RouteMessage tries to call it, it will fail

    register_fake_session(tx_uuid, "BOUND_TX");

    auto client = sdbus::createProxy(*server_conn, server_svc, ISERVER_PATH);
    std::string routed_to;
    client->callMethod("RouteMessage").onInterface(ISERVER_IFACE)
          .withArguments(std::string("src"), std::string("dst"),
                         std::string("hi"), std::string("id-y"))
          .storeResultsTo(routed_to);
    // Must not route to the TX-only session
    EXPECT_TRUE(routed_to.empty());
}

TEST_F(RouteMessageTest, RouteMessageWorksForBoundTrxSession) {
    const std::string trx_uuid = "trx-" + suffix;
    const std::string trx_svc  = "com.telecom.smpp.Client." + trx_uuid;

    ReceiverStub receiver(trx_svc);
    register_fake_session(trx_uuid, "BOUND_TRX");

    auto client = sdbus::createProxy(*server_conn, server_svc, ISERVER_PATH);
    std::string routed_to;
    client->callMethod("RouteMessage").onInterface(ISERVER_IFACE)
          .withArguments(std::string("src"), std::string("trx"),
                         std::string("test"), std::string("id-z"))
          .storeResultsTo(routed_to);

    EXPECT_EQ(routed_to, trx_uuid);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(receiver.deliver_count.load(), 1);
}
