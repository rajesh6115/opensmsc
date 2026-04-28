#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include "common/config.hpp"

static const char* kTestToml = R"(
[server]
port = 2775
ip_whitelist = "/etc/smpp-server/allowed_ips.conf"
max_connections_per_ip = 5

[session]
enquire_link_interval_sec = 60
enquire_link_timeout_sec = 10

[auth]
credentials_file = "/etc/smpp-server/credentials.toml"

[logging]
level = "info"
)";

class ConfigTest : public ::testing::Test {
protected:
    std::string tmp_path;

    void SetUp() override {
        tmp_path = "/tmp/test_smpp_config.toml";
        std::ofstream f(tmp_path);
        f << kTestToml;
    }

    void TearDown() override {
        std::remove(tmp_path.c_str());
        unsetenv("SMPP_PORT");
    }
};

TEST_F(ConfigTest, LoadsPortFromFile) {
    auto cfg = smpp::Config::load(tmp_path);
    EXPECT_EQ(cfg.server.port, 2775);
}

TEST_F(ConfigTest, LoadsMaxConnectionsPerIp) {
    auto cfg = smpp::Config::load(tmp_path);
    EXPECT_EQ(cfg.server.max_connections_per_ip, 5);
}

TEST_F(ConfigTest, LoadsEnquireLinkSettings) {
    auto cfg = smpp::Config::load(tmp_path);
    EXPECT_EQ(cfg.session.enquire_link_interval_sec, 60);
    EXPECT_EQ(cfg.session.enquire_link_timeout_sec, 10);
}

TEST_F(ConfigTest, LoadsLoggingLevel) {
    auto cfg = smpp::Config::load(tmp_path);
    EXPECT_EQ(cfg.logging.level, "info");
}

TEST_F(ConfigTest, EnvVarOverridesPort) {
    setenv("SMPP_PORT", "9999", 1);
    auto cfg = smpp::Config::load(tmp_path);
    EXPECT_EQ(cfg.server.port, 9999);
}

TEST_F(ConfigTest, ThrowsOnMissingFile) {
    EXPECT_THROW(smpp::Config::load("/nonexistent/path.toml"), std::runtime_error);
}
