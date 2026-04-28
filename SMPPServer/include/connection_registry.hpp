#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

struct SessionEntry {
    int         fd;
    std::string client_ip;
};

class ConnectionRegistry {
public:
    explicit ConnectionRegistry(uint32_t max_per_ip = 5);

    bool                add(const std::string& uuid, int fd, const std::string& client_ip);
    std::optional<int>  claim_fd(const std::string& uuid);
    void                remove(const std::string& uuid);
    uint32_t            count(const std::string& ip) const;
    void                set_max_per_ip(uint32_t max);
    uint32_t            max_per_ip() const;

private:
    mutable std::mutex                            mutex_;
    uint32_t                                      max_per_ip_;
    std::unordered_map<std::string, SessionEntry> sessions_;
    std::unordered_map<std::string, uint32_t>     ip_counts_;
};
