#pragma once

#include "connection_registry.hpp"
#include "generated/IServer_adaptor.hpp"

#include <sdbus-c++/sdbus-c++.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct SessionInfo {
    std::string client_ip;
    std::string system_id;
    std::string state;
};

// Implements the IServer D-Bus adaptor.
// Caller owns the sdbus::IObject and must call obj.finishRegistration() after construction.
class SmppServerService : public com::telecom::smpp::IServer_adaptor {
public:
    static constexpr const char* SERVICE_NAME = "com.telecom.smpp.Server";
    static constexpr const char* OBJ_PATH     = "/com/telecom/smpp/server";

    SmppServerService(sdbus::IObject&                      obj,
                      std::shared_ptr<ConnectionRegistry>  registry);

    // Called by TcpServer on each accepted connection; emits SessionStarted signal.
    void register_session(const std::string& uuid, int fd, const std::string& client_ip);

    // Called when a session ends (e.g. SmppClientHandler exits).
    void unregister_session(const std::string& uuid);

private:
    sdbus::UnixFd GetSocket(const std::string& uuid) override;
    void UpdateSessionDetails(const std::string& uuid,
                              const std::string& system_id,
                              const std::string& state) override;
    uint32_t GetConnectionCount(const std::string& ip) override;
    std::vector<sdbus::Struct<std::string,std::string,std::string,std::string>>
        GetAllSessions() override;
    void SetMaxConnectionsPerIp(const std::string& ip, const uint32_t& max) override;
    void DisconnectAll(const std::string& ip, const std::string& reason) override;

    std::shared_ptr<ConnectionRegistry>  registry_;
    mutable std::mutex                   sessions_mutex_;
    std::map<std::string, SessionInfo>   sessions_;
};
