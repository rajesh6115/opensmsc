#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <cstdio>

#include "credentials_store.hpp"
#include "auth_service.hpp"

static constexpr const char* SVC  = "com.telecom.smpp.Auth.test";
static constexpr const char* PATH = "/com/telecom/smpp/auth";
static constexpr const char* IFACE = "com.telecom.smpp.IAuth";

static std::string write_creds(const char* content) {
    const std::string path = "/tmp/test_credentials.toml";
    std::ofstream f(path); f << content;
    return path;
}

// ── CredentialsStore unit tests ───────────────────────────────────────────────

TEST(CredentialsStore, AuthenticateSuccess) {
    auto p = write_creds("[[credentials]]\nsystem_id=\"s1\"\npassword=\"p1\"\n");
    CredentialsStore store(p);
    auto [ok, code] = store.authenticate("s1", "p1");
    EXPECT_TRUE(ok);
    EXPECT_EQ(code, CredentialsStore::OK);
}

TEST(CredentialsStore, UnknownSystemId) {
    auto p = write_creds("[[credentials]]\nsystem_id=\"s1\"\npassword=\"p1\"\n");
    CredentialsStore store(p);
    auto [ok, code] = store.authenticate("no-such", "p1");
    EXPECT_FALSE(ok);
    EXPECT_EQ(code, CredentialsStore::ESME_RINVSYSID);
}

TEST(CredentialsStore, WrongPassword) {
    auto p = write_creds("[[credentials]]\nsystem_id=\"s1\"\npassword=\"p1\"\n");
    CredentialsStore store(p);
    auto [ok, code] = store.authenticate("s1", "wrong");
    EXPECT_FALSE(ok);
    EXPECT_EQ(code, CredentialsStore::ESME_RINVPASWD);
}

TEST(CredentialsStore, Reload) {
    auto p = write_creds("[[credentials]]\nsystem_id=\"s1\"\npassword=\"p1\"\n");
    CredentialsStore store(p);
    { std::ofstream f(p); f << "[[credentials]]\nsystem_id=\"s2\"\npassword=\"p2\"\n"; }
    store.reload();
    auto [ok1, _1] = store.authenticate("s1", "p1");
    auto [ok2, _2] = store.authenticate("s2", "p2");
    EXPECT_FALSE(ok1);
    EXPECT_TRUE(ok2);
}

// ── AuthService D-Bus tests ───────────────────────────────────────────────────

class AuthServiceDbusTest : public ::testing::Test {
protected:
    std::unique_ptr<sdbus::IConnection> conn;
    std::unique_ptr<sdbus::IObject>     obj;
    std::shared_ptr<CredentialsStore>   store;
    std::unique_ptr<AuthService>        svc;
    std::thread                         event_thread;
    std::string                         creds_path;

    void SetUp() override {
        creds_path = write_creds(
            "[[credentials]]\nsystem_id=\"esme1\"\npassword=\"pass1\"\n");
        store = std::make_shared<CredentialsStore>(creds_path);
        conn  = sdbus::createSessionBusConnection(SVC);
        obj   = sdbus::createObject(*conn, PATH);
        svc   = std::make_unique<AuthService>(*obj, store);
        obj->finishRegistration();
        event_thread = std::thread([this]{ conn->enterEventLoopAsync(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        conn->leaveEventLoop();
        if (event_thread.joinable()) event_thread.join();
    }
};

TEST_F(AuthServiceDbusTest, AuthenticateValidCredentials) {
    auto pc = sdbus::createSessionBusConnection();
    auto px = sdbus::createProxy(*pc, SVC, PATH);
    bool ok{}; uint32_t code{};
    px->callMethod("Authenticate").onInterface(IFACE)
      .withArguments(std::string("uuid1"), std::string("esme1"),
                     std::string("pass1"), std::string("127.0.0.1"))
      .storeResultsTo(ok, code);
    EXPECT_TRUE(ok);
    EXPECT_EQ(code, 0u);
}

TEST_F(AuthServiceDbusTest, AuthenticateInvalidPassword) {
    auto pc = sdbus::createSessionBusConnection();
    auto px = sdbus::createProxy(*pc, SVC, PATH);
    bool ok{}; uint32_t code{};
    px->callMethod("Authenticate").onInterface(IFACE)
      .withArguments(std::string("uuid2"), std::string("esme1"),
                     std::string("wrong"), std::string("127.0.0.1"))
      .storeResultsTo(ok, code);
    EXPECT_FALSE(ok);
    EXPECT_EQ(code, CredentialsStore::ESME_RINVPASWD);
}

TEST_F(AuthServiceDbusTest, ReloadCredentials) {
    std::ofstream f(creds_path);
    f << "[[credentials]]\nsystem_id=\"new\"\npassword=\"newpass\"\n";
    f.close();

    auto pc = sdbus::createSessionBusConnection();
    auto px = sdbus::createProxy(*pc, SVC, PATH);
    px->callMethod("ReloadCredentials").onInterface(IFACE).dontExpectReply();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool ok{}; uint32_t code{};
    px->callMethod("Authenticate").onInterface(IFACE)
      .withArguments(std::string("u"), std::string("new"),
                     std::string("newpass"), std::string("1.1.1.1"))
      .storeResultsTo(ok, code);
    EXPECT_TRUE(ok);
}
