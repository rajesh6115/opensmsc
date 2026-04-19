#include "ip_validator.hpp"

#include "logger.hpp"

#include <arpa/inet.h>   // inet_pton

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string_view>

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string trim(std::string s)
{
    auto not_space = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

// ── IpValidator ──────────────────────────────────────────────────────────────

IpValidator::IpValidator(const std::string& config_file)
    : config_file_(config_file)
{
    load_config();
}

bool IpValidator::parse_ipv4(const std::string& s, uint32_t& out)
{
    struct in_addr addr{};
    if (inet_pton(AF_INET, s.c_str(), &addr) == 1) {
        out = ntohl(addr.s_addr);   // store in host-byte order
        return true;
    }
    return false;
}

void IpValidator::load_config()
{
    exact_set_.clear();
    cidrs_.clear();

    std::ifstream f(config_file_);
    if (!f.is_open()) {
        throw std::runtime_error("IpValidator: cannot open config file: " + config_file_);
    }

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto slash = line.find('/');
        if (slash != std::string::npos) {
            // ── IPv4 CIDR ────────────────────────────────────────────────────
            std::string network_str = line.substr(0, slash);
            int prefix = std::stoi(line.substr(slash + 1));
            if (prefix < 0 || prefix > 32) {
                LOG_WARN("IpValidator", "invalid prefix length in: {} — skipping", line);
                continue;
            }
            uint32_t net{};
            if (!parse_ipv4(network_str, net)) {
                LOG_WARN("IpValidator", "invalid network address in: {} — skipping", line);
                continue;
            }
            uint32_t mask = (prefix == 0) ? 0u : (~uint32_t{0} << (32 - prefix));
            cidrs_.push_back({ net & mask, mask });
        } else {
            // ── Exact match (IPv4 or IPv6) ───────────────────────────────────
            exact_set_.insert(line);
        }
    }

    LOG_INFO("IpValidator", "loaded {} exact entries and {} CIDR ranges from {}",
             exact_set_.size(), cidrs_.size(), config_file_);
}

bool IpValidator::matches_cidr(uint32_t addr) const
{
    for (const auto& c : cidrs_) {
        if ((addr & c.mask) == c.network) return true;
    }
    return false;
}

bool IpValidator::is_allowed(const std::string& ip) const
{
    // 1. Exact match (covers both IPv4 and IPv6).
    if (exact_set_.count(ip)) return true;

    // 2. IPv4 CIDR match.
    uint32_t addr{};
    if (parse_ipv4(ip, addr)) {
        if (matches_cidr(addr)) return true;
    }

    return false;
}

void IpValidator::reload()
{
    LOG_INFO("IpValidator", "reloading config");
    load_config();
}
