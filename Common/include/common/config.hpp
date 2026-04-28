#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>

namespace smpp {

struct ServerConfig {
    uint16_t    port                  = 2775;
    std::string ip_whitelist          = "/etc/smpp-server/allowed_ips.conf";
    uint32_t    max_connections_per_ip = 5;
};

struct SessionConfig {
    uint32_t enquire_link_interval_sec = 60;
    uint32_t enquire_link_timeout_sec  = 10;
};

struct AuthConfig {
    std::string credentials_file = "/etc/smpp-server/credentials.toml";
};

struct LoggingConfig {
    std::string level = "info";
};

struct Config {
    ServerConfig  server;
    SessionConfig session;
    AuthConfig    auth;
    LoggingConfig logging;

    static Config load(const std::string& path);
};

} // namespace smpp
