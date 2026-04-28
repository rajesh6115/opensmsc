#include <gtest/gtest.h>
#include "session_launcher.hpp"

TEST(SessionLauncher, CommandContainsUuidAndIp) {
    const std::string cmd = build_launch_command("abc-123", "10.0.0.5");
    EXPECT_NE(cmd.find("smpp-client-abc-123"), std::string::npos);
    EXPECT_NE(cmd.find("--uuid=abc-123"),      std::string::npos);
    EXPECT_NE(cmd.find("--client-ip=10.0.0.5"), std::string::npos);
    EXPECT_NE(cmd.find("systemd-run"),          std::string::npos);
    EXPECT_NE(cmd.find("smpp_client_handler"),  std::string::npos);
}

TEST(SessionLauncher, CommandContainsCollectFlag) {
    const std::string cmd = build_launch_command("x", "1.2.3.4");
    EXPECT_NE(cmd.find("--collect"), std::string::npos);
}
