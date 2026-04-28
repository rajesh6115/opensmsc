#include "common/config.hpp"

#include <toml++/toml.hpp>
#include <cstdlib>

namespace smpp {

static uint16_t env_uint16(const char* var, uint16_t fallback) {
    const char* v = std::getenv(var);
    return v ? static_cast<uint16_t>(std::stoul(v)) : fallback;
}

Config Config::load(const std::string& path) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("Failed to parse config: ") + e.what());
    }

    Config cfg;

    if (auto s = tbl["server"].as_table()) {
        if (auto v = (*s)["port"].value<int64_t>())               cfg.server.port                   = static_cast<uint16_t>(*v);
        if (auto v = (*s)["ip_whitelist"].value<std::string>())   cfg.server.ip_whitelist           = *v;
        if (auto v = (*s)["max_connections_per_ip"].value<int64_t>()) cfg.server.max_connections_per_ip = static_cast<uint32_t>(*v);
    }

    if (auto s = tbl["session"].as_table()) {
        if (auto v = (*s)["enquire_link_interval_sec"].value<int64_t>()) cfg.session.enquire_link_interval_sec = static_cast<uint32_t>(*v);
        if (auto v = (*s)["enquire_link_timeout_sec"].value<int64_t>())  cfg.session.enquire_link_timeout_sec  = static_cast<uint32_t>(*v);
    }

    if (auto s = tbl["auth"].as_table()) {
        if (auto v = (*s)["credentials_file"].value<std::string>()) cfg.auth.credentials_file = *v;
    }

    if (auto s = tbl["logging"].as_table()) {
        if (auto v = (*s)["level"].value<std::string>()) cfg.logging.level = *v;
    }

    // Environment variable overrides
    cfg.server.port = env_uint16("SMPP_PORT", cfg.server.port);

    return cfg;
}

} // namespace smpp
