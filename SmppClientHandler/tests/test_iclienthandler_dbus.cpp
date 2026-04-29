#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <cstdlib>

#include "generated/IClientHandler_adaptor.hpp"

static constexpr const char* SERVICE  = "com.telecom.smpp.Client.test-uuid";
static constexpr const char* OBJ_PATH = "/com/telecom/smpp/client";

class IClientHandlerStub : public com::telecom::smpp::IClientHandler_adaptor {
public:
    IClientHandlerStub(sdbus::IObject& obj) : IClientHandler_adaptor(obj) {}
private:
    std::tuple<std::string,std::string,std::string,std::string,uint64_t>
    GetSessionInfo() override {
        return {"test-uuid", "127.0.0.1", "test-client", "BOUND_TX", 0ULL};
    }
    void Disconnect(const std::string&) override {}
    uint32_t DeliverSm(const std::string&, const std::string&,
                       const std::string&) override { return 1u; }
};

class IClientHandlerDbusTest : public ::testing::Test {
protected:
    std::unique_ptr<sdbus::IConnection> conn;
    std::unique_ptr<sdbus::IObject>     obj;
    std::unique_ptr<IClientHandlerStub> stub;
    std::thread                         event_thread;

    void SetUp() override {
        conn = sdbus::createSessionBusConnection(SERVICE);
        obj  = sdbus::createObject(*conn, OBJ_PATH);
        stub = std::make_unique<IClientHandlerStub>(*obj);
        obj->finishRegistration();
        event_thread = std::thread([this] { conn->enterEventLoopAsync(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        conn->leaveEventLoop();
        if (event_thread.joinable()) event_thread.join();
    }
};

TEST_F(IClientHandlerDbusTest, ServiceAppearsOnSessionBus) {
    int rc = std::system("busctl --user list 2>/dev/null | grep -q 'com.telecom.smpp.Client'");
    EXPECT_EQ(rc, 0);
}

TEST_F(IClientHandlerDbusTest, InterfaceIntrospects) {
    int rc = std::system(
        "busctl --user introspect com.telecom.smpp.Client.test-uuid "
        "/com/telecom/smpp/client 2>/dev/null | grep -q 'com.telecom.smpp.IClientHandler'");
    EXPECT_EQ(rc, 0);
}

TEST_F(IClientHandlerDbusTest, AllMethodsAndSignalsVisible) {
    auto check = [](const char* m) {
        std::string cmd = std::string(
            "busctl --user introspect com.telecom.smpp.Client.test-uuid "
            "/com/telecom/smpp/client 2>/dev/null | grep -q '") + m + "'";
        return std::system(cmd.c_str()) == 0;
    };
    EXPECT_TRUE(check("GetSessionInfo"));
    EXPECT_TRUE(check("Disconnect"));
    EXPECT_TRUE(check("DeliverSm"));
    EXPECT_TRUE(check("SessionExited"));
}
