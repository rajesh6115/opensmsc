#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

// Validates an incoming IP address against a whitelist that supports
// both exact addresses (IPv4 / IPv6) and IPv4 CIDR ranges.
//
// Config file format (one entry per line):
//   127.0.0.1
//   ::1
//   192.168.1.0/24
//   10.0.0.0/8
//   # lines starting with '#' and blank lines are ignored
class IpValidator
{
public:
    explicit IpValidator(const std::string& config_file);

    // Returns true if 'ip' (dotted-decimal or colon-hex string) is allowed.
    bool is_allowed(const std::string& ip) const;

    // Re-reads the config file without restarting the server.
    void reload();

private:
    struct CidrV4
    {
        uint32_t network;   // host-byte order
        uint32_t mask;      // host-byte order, e.g. 0xFFFFFF00 for /24
    };

    void load_config();
    static bool parse_ipv4(const std::string& s, uint32_t& out);
    bool matches_cidr(uint32_t addr) const;

    std::string                  config_file_;
    std::unordered_set<std::string> exact_set_;   // exact IPv4 / IPv6 strings
    std::vector<CidrV4>          cidrs_;           // IPv4 CIDR ranges
};
