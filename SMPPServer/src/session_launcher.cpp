#include "session_launcher.hpp"
#include "logger.hpp"

#include <cstdlib>

static constexpr const char* HANDLER_BIN = "/usr/local/bin/smpp_client_handler";

std::string build_launch_command(const std::string& uuid, const std::string& client_ip)
{
    return std::string("systemd-run")
        + " --unit=smpp-client-" + uuid
        + " --collect"
        + " --property=User=smpp-client"
        + " " + HANDLER_BIN
        + " --uuid=" + uuid
        + " --client-ip=" + client_ip;
}

bool launch_client_handler(const std::string& uuid, const std::string& client_ip)
{
    const std::string cmd = build_launch_command(uuid, client_ip);
    LOG_INFO("Launcher", "spawning: {}", cmd);
    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        LOG_ERROR("Launcher", "systemd-run failed rc={} uuid={}", rc, uuid);
        return false;
    }
    return true;
}
