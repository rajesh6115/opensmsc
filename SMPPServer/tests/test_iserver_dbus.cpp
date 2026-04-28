#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib>

#include "generated/IServer_adaptor.hpp"
#include "generated/IServer_proxy.hpp"

// Minimal stub implementing all pure-virtual methods
class IServerStub : public com::telecom::smpp::IServer_adaptor {
public:
    IServerStub(sdbus::IObject& obj) : IServer_adaptor(obj) {}

private:
    sdbus::UnixFd GetSocket(const std::string&) override {
        return sdbus::UnixFd{};
    }
    void UpdateSessionDetails(const std::string&, const std::string&, const std::string&) override {}
    uint32_t GetConnectionCount(const std::string&) override { return 0; }
    std::vector<sdbus::Struct<std::string,std::string,std::string,std::string>> GetAllSessions() override { return {}; }
    void SetMaxConnectionsPerIp(const std::string&, const uint32_t&) override {}
    void DisconnectAll(const std::string&, const std::string&) override {}
};

static constexpr const char* SERVICE  = "com.telecom.smpp.Server";
static constexpr const char* OBJ_PATH = "/com/telecom/smpp/server";

class IServerDbusTest : public ::testing::Test {
protected:
    std::unique_ptr<sdbus::IConnection> conn;
    std::unique_ptr<sdbus::IObject>     obj;
    std::unique_ptr<IServerStub>        stub;
    std::thread                         event_thread;
    std::atomic<bool>                   running{false};

    void SetUp() override {
        conn = sdbus::createSessionBusConnection(SERVICE);
        obj  = sdbus::createObject(*conn, OBJ_PATH);
        stub = std::make_unique<IServerStub>(*obj);
        obj->finishRegistration();

        running = true;
        event_thread = std::thread([this] {
            conn->enterEventLoopAsync();
        });
        // Give the event loop time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        conn->leaveEventLoop();
        if (event_thread.joinable()) event_thread.join();
    }
};

TEST_F(IServerDbusTest, ServiceAppearsOnSessionBus) {
    // busctl --user list should show our service
    int rc = std::system("busctl --user list 2>/dev/null | grep -q 'com.telecom.smpp.Server'");
    EXPECT_EQ(rc, 0) << "Service com.telecom.smpp.Server not found on session bus";
}

TEST_F(IServerDbusTest, InterfaceIntrospects) {
    // busctl --user introspect should show IServer interface
    int rc = std::system(
        "busctl --user introspect com.telecom.smpp.Server "
        "/com/telecom/smpp/server 2>/dev/null | grep -q 'com.telecom.smpp.IServer'"
    );
    EXPECT_EQ(rc, 0) << "Interface com.telecom.smpp.IServer not found via introspection";
}

TEST_F(IServerDbusTest, AllMethodsVisible) {
    auto check = [](const char* method) {
        std::string cmd = std::string(
            "busctl --user introspect com.telecom.smpp.Server "
            "/com/telecom/smpp/server 2>/dev/null | grep -q '") + method + "'";
        return std::system(cmd.c_str()) == 0;
    };
    EXPECT_TRUE(check("GetSocket"));
    EXPECT_TRUE(check("UpdateSessionDetails"));
    EXPECT_TRUE(check("GetConnectionCount"));
    EXPECT_TRUE(check("GetAllSessions"));
    EXPECT_TRUE(check("SetMaxConnectionsPerIp"));
    EXPECT_TRUE(check("DisconnectAll"));
    EXPECT_TRUE(check("SessionStarted"));
}
