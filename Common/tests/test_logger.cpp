#include <gtest/gtest.h>
#include "common/logger.hpp"

TEST(LoggerTest, InitialisesWithoutThrowing) {
    EXPECT_NO_THROW(smpp::Logger::init("test-service"));
}

TEST(LoggerTest, GetReturnsValidLogger) {
    smpp::Logger::init("test-service");
    auto log = smpp::Logger::get();
    ASSERT_NE(log, nullptr);
}

TEST(LoggerTest, LogsWithSessionContext) {
    smpp::Logger::init("test-service");
    smpp::LogContext ctx;
    ctx.session_id = "550e8400-e29b-41d4-a716-446655440000";
    ctx.client_ip  = "127.0.0.1";
    ctx.system_id  = "test-client";
    EXPECT_NO_THROW(smpp::Logger::info(ctx, "test message"));
}

TEST(LoggerTest, LogsWithoutSessionContext) {
    smpp::Logger::init("test-service");
    EXPECT_NO_THROW(smpp::Logger::info("startup message"));
}
