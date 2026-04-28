#include "smpp_server_service.hpp"
#include "logger.hpp"

SmppServerService::SmppServerService(sdbus::IObject&                     obj,
                                     std::shared_ptr<ConnectionRegistry> registry)
    : IServer_adaptor(obj)
    , registry_(std::move(registry))
{}

void SmppServerService::register_session(const std::string& uuid,
                                         int fd,
                                         const std::string& client_ip)
{
    if (!registry_->add(uuid, fd, client_ip)) {
        LOG_WARN("IServer", "register_session rejected: IP {} at max connections", client_ip);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[uuid] = {client_ip, "", "OPEN"};
    }
    emitSessionStarted(uuid, client_ip);
    LOG_INFO("IServer", "session registered uuid={} ip={}", uuid, client_ip);
}

void SmppServerService::unregister_session(const std::string& uuid)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(uuid);
}

sdbus::UnixFd SmppServerService::GetSocket(const std::string& uuid)
{
    auto fd_opt = registry_->claim_fd(uuid);
    if (!fd_opt.has_value())
        throw sdbus::Error("com.telecom.smpp.Error.NotFound", "Unknown uuid: " + uuid);
    LOG_INFO("IServer", "GetSocket uuid={} fd={}", uuid, *fd_opt);
    return sdbus::UnixFd{*fd_opt};
}

void SmppServerService::UpdateSessionDetails(const std::string& uuid,
                                             const std::string& system_id,
                                             const std::string& state)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(uuid);
    if (it != sessions_.end()) {
        it->second.system_id = system_id;
        it->second.state     = state;
    }
    LOG_INFO("IServer", "UpdateSessionDetails uuid={} system_id={} state={}",
             uuid, system_id, state);
}

uint32_t SmppServerService::GetConnectionCount(const std::string& ip)
{
    return registry_->count(ip);
}

std::vector<sdbus::Struct<std::string,std::string,std::string,std::string>>
SmppServerService::GetAllSessions()
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<sdbus::Struct<std::string,std::string,std::string,std::string>> result;
    result.reserve(sessions_.size());
    for (const auto& [uuid, info] : sessions_)
        result.push_back({uuid, info.client_ip, info.system_id, info.state});
    return result;
}

void SmppServerService::SetMaxConnectionsPerIp(const std::string& /*ip*/, const uint32_t& max)
{
    registry_->set_max_per_ip(max);
    LOG_INFO("IServer", "SetMaxConnectionsPerIp max={}", max);
}

void SmppServerService::DisconnectAll(const std::string& ip, const std::string& reason)
{
    LOG_INFO("IServer", "DisconnectAll ip={} reason={} (TODO: proxy IClientHandler)", ip, reason);
}
