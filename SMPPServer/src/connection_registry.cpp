#include "connection_registry.hpp"

ConnectionRegistry::ConnectionRegistry(uint32_t max_per_ip)
    : max_per_ip_(max_per_ip)
{}

bool ConnectionRegistry::add(const std::string& uuid, int fd, const std::string& client_ip)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& cnt = ip_counts_[client_ip];
    if (cnt >= max_per_ip_) return false;
    sessions_[uuid] = {fd, client_ip};
    ++cnt;
    return true;
}

std::optional<int> ConnectionRegistry::claim_fd(const std::string& uuid)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(uuid);
    if (it == sessions_.end()) return std::nullopt;
    int fd = it->second.fd;
    auto& cnt = ip_counts_[it->second.client_ip];
    if (cnt > 0) --cnt;
    sessions_.erase(it);
    return fd;
}

void ConnectionRegistry::remove(const std::string& uuid)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(uuid);
    if (it == sessions_.end()) return;
    auto& cnt = ip_counts_[it->second.client_ip];
    if (cnt > 0) --cnt;
    sessions_.erase(it);
}

uint32_t ConnectionRegistry::count(const std::string& ip) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ip_counts_.find(ip);
    return (it != ip_counts_.end()) ? it->second : 0u;
}

void ConnectionRegistry::set_max_per_ip(uint32_t max)
{
    std::lock_guard<std::mutex> lock(mutex_);
    max_per_ip_ = max;
}

uint32_t ConnectionRegistry::max_per_ip() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return max_per_ip_;
}
