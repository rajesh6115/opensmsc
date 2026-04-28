#include <gtest/gtest.h>
#include "connection_registry.hpp"

TEST(ConnectionRegistry, AddAndClaimFd) {
    ConnectionRegistry reg(5);
    EXPECT_TRUE(reg.add("uuid-1", 42, "192.168.1.1"));
    EXPECT_EQ(reg.count("192.168.1.1"), 1u);
    auto fd = reg.claim_fd("uuid-1");
    ASSERT_TRUE(fd.has_value());
    EXPECT_EQ(*fd, 42);
    EXPECT_EQ(reg.count("192.168.1.1"), 0u);
}

TEST(ConnectionRegistry, ClaimUnknownReturnsNullopt) {
    ConnectionRegistry reg(5);
    EXPECT_FALSE(reg.claim_fd("no-such-uuid").has_value());
}

TEST(ConnectionRegistry, Remove) {
    ConnectionRegistry reg(5);
    reg.add("uuid-2", 10, "10.0.0.1");
    EXPECT_EQ(reg.count("10.0.0.1"), 1u);
    reg.remove("uuid-2");
    EXPECT_EQ(reg.count("10.0.0.1"), 0u);
}

TEST(ConnectionRegistry, EnforcesMaxPerIp) {
    ConnectionRegistry reg(2);
    EXPECT_TRUE(reg.add("a", 1, "1.2.3.4"));
    EXPECT_TRUE(reg.add("b", 2, "1.2.3.4"));
    EXPECT_FALSE(reg.add("c", 3, "1.2.3.4"));
    EXPECT_EQ(reg.count("1.2.3.4"), 2u);
}

TEST(ConnectionRegistry, SetMaxPerIp) {
    ConnectionRegistry reg(1);
    reg.add("x", 5, "5.5.5.5");
    EXPECT_FALSE(reg.add("y", 6, "5.5.5.5"));
    reg.set_max_per_ip(3);
    EXPECT_TRUE(reg.add("y", 6, "5.5.5.5"));
}

TEST(ConnectionRegistry, DifferentIpsAreIndependent) {
    ConnectionRegistry reg(1);
    EXPECT_TRUE(reg.add("p", 1, "1.1.1.1"));
    EXPECT_TRUE(reg.add("q", 2, "2.2.2.2"));
    EXPECT_FALSE(reg.add("r", 3, "1.1.1.1"));
}
