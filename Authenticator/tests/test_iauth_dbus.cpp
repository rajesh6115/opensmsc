#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <cstdlib>

#include "generated/IAuth_adaptor.hpp"

static constexpr const char* SERVICE  = "com.telecom.smpp.Auth";
static constexpr const char* OBJ_PATH = "/com/telecom/smpp/auth";

class IAuthStub : public com::telecom::smpp::IAuth_adaptor {
public:
    IAuthStub(sdbus::IObject& obj) : IAuth_adaptor(obj) {}
private:
    std::tuple<bool, uint32_t> Authenticate(
        const std::string&, const std::string&,
        const std::string&, const std::string&) override {
        return {true, 0};
    }
    void ReloadCredentials() override {}
};

class IAuthDbusTest : public ::testing::Test {
protected:
    std::unique_ptr<sdbus::IConnection> conn;
    std::unique_ptr<sdbus::IObject>     obj;
    std::unique_ptr<IAuthStub>          stub;
    std::thread                         event_thread;

    void SetUp() override {
        conn = sdbus::createSessionBusConnection(SERVICE);
        obj  = sdbus::createObject(*conn, OBJ_PATH);
        stub = std::make_unique<IAuthStub>(*obj);
        obj->finishRegistration();
        event_thread = std::thread([this] { conn->enterEventLoopAsync(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        conn->leaveEventLoop();
        if (event_thread.joinable()) event_thread.join();
    }
};

TEST_F(IAuthDbusTest, ServiceAppearsOnSessionBus) {
    int rc = std::system("busctl --user list 2>/dev/null | grep -q 'com.telecom.smpp.Auth'");
    EXPECT_EQ(rc, 0);
}

TEST_F(IAuthDbusTest, InterfaceIntrospects) {
    int rc = std::system(
        "busctl --user introspect com.telecom.smpp.Auth "
        "/com/telecom/smpp/auth 2>/dev/null | grep -q 'com.telecom.smpp.IAuth'");
    EXPECT_EQ(rc, 0);
}

TEST_F(IAuthDbusTest, AllMethodsVisible) {
    auto check = [](const char* m) {
        std::string cmd = std::string(
            "busctl --user introspect com.telecom.smpp.Auth "
            "/com/telecom/smpp/auth 2>/dev/null | grep -q '") + m + "'";
        return std::system(cmd.c_str()) == 0;
    };
    EXPECT_TRUE(check("Authenticate"));
    EXPECT_TRUE(check("ReloadCredentials"));
}
