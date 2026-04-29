#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <unistd.h>

#include "connection_registry.hpp"
#include "smpp_server_service.hpp"
#include "generated/IServer_proxy.hpp"

static constexpr const char* SVC  = "com.telecom.smpp.Server.test";
static constexpr const char* PATH = "/com/telecom/smpp/server";

class IServerMethodsTest : public ::testing::Test {
protected:
    std::unique_ptr<sdbus::IConnection>   conn;
    std::unique_ptr<sdbus::IObject>       obj;
    std::shared_ptr<ConnectionRegistry>   registry;
    std::unique_ptr<SmppServerService>    svc;
    std::thread                           event_thread;

    void SetUp() override {
        conn     = sdbus::createSessionBusConnection(SVC);
        obj      = sdbus::createObject(*conn, PATH);
        registry = std::make_shared<ConnectionRegistry>(5);
        svc      = std::make_unique<SmppServerService>(*obj, *conn, registry);
        obj->finishRegistration();
        event_thread = std::thread([this]{ conn->enterEventLoopAsync(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        conn->leaveEventLoop();
        if (event_thread.joinable()) event_thread.join();
    }
};

TEST_F(IServerMethodsTest, GetConnectionCountZeroInitially) {
    auto proxy_conn = sdbus::createSessionBusConnection();
    auto proxy = sdbus::createProxy(*proxy_conn, SVC, PATH);
    uint32_t cnt{};
    proxy->callMethod("GetConnectionCount").onInterface("com.telecom.smpp.IServer")
         .withArguments(std::string("10.0.0.1")).storeResultsTo(cnt);
    EXPECT_EQ(cnt, 0u);
}

TEST_F(IServerMethodsTest, GetSocketClaimsFd) {
    // Create a pipe so we have a real fd to store
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);
    const std::string uuid = "test-uuid-001";
    svc->register_session(uuid, pipefd[0], "127.0.0.1");

    auto proxy_conn = sdbus::createSessionBusConnection();
    auto proxy = sdbus::createProxy(*proxy_conn, SVC, PATH);

    sdbus::UnixFd received_fd;
    proxy->callMethod("GetSocket").onInterface("com.telecom.smpp.IServer")
         .withArguments(uuid).storeResultsTo(received_fd);
    EXPECT_GE(received_fd.get(), 0);

    // After claim the registry count should be 0
    EXPECT_EQ(registry->count("127.0.0.1"), 0u);

    close(received_fd.get());
    close(pipefd[1]);
}

TEST_F(IServerMethodsTest, GetSocketUnknownUuidThrows) {
    auto proxy_conn = sdbus::createSessionBusConnection();
    auto proxy = sdbus::createProxy(*proxy_conn, SVC, PATH);
    sdbus::UnixFd fd;
    EXPECT_THROW(
        proxy->callMethod("GetSocket").onInterface("com.telecom.smpp.IServer")
             .withArguments(std::string("no-such-uuid")).storeResultsTo(fd),
        sdbus::Error
    );
}

TEST_F(IServerMethodsTest, UpdateAndGetAllSessions) {
    int pipefd[2]; pipe(pipefd);
    svc->register_session("s1", pipefd[0], "1.2.3.4");
    close(pipefd[1]);

    auto proxy_conn = sdbus::createSessionBusConnection();
    auto proxy = sdbus::createProxy(*proxy_conn, SVC, PATH);

    proxy->callMethod("UpdateSessionDetails").onInterface("com.telecom.smpp.IServer")
         .withArguments(std::string("s1"), std::string("esme1"), std::string("BOUND_TX"));

    using Row = sdbus::Struct<std::string,std::string,std::string,std::string>;
    std::vector<Row> sessions;
    proxy->callMethod("GetAllSessions").onInterface("com.telecom.smpp.IServer")
         .storeResultsTo(sessions);

    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(std::get<0>(sessions[0]), "s1");
    EXPECT_EQ(std::get<2>(sessions[0]), "esme1");
    EXPECT_EQ(std::get<3>(sessions[0]), "BOUND_TX");

    // Claim the fd to avoid leak
    registry->claim_fd("s1");
}
