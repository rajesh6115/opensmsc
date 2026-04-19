#include "smpp_service_manager.hpp"

#include "logger.hpp"
#include <string>

// ── SmppServiceManager ────────────────────────────────────────────────────────
//
// Phase 1.1: Basic implementation for SMPP protocol operations.
// D-Bus integration (systemd unit management) will be added in Phase 2+.

struct SmppServiceManager::Impl
{
    // Placeholder for Phase 2+ D-Bus integration
    // For now, this is a stub implementation focusing on SMPP operations.
};

SmppServiceManager::SmppServiceManager()
    : impl_{std::make_unique<Impl>()}
{
    LOG_INFO("SmppServiceManager", "initialized (Phase 1.1 MVP)");
}

SmppServiceManager::~SmppServiceManager() = default;

// ── Public API ────────────────────────────────────────────────────────────────

bool SmppServiceManager::start_authenticator(const std::string& session_id,
                                              const std::string& client_ip)
{
    LOG_INFO("SmppServiceManager", "authenticator start requested session_id={} client_ip={}",
             session_id, client_ip);
    // Phase 1.1: Basic logging only
    // Phase 2+: Will integrate with systemd via D-Bus
    return true;
}

bool SmppServiceManager::stop_authenticator(const std::string& session_id)
{
    LOG_INFO("SmppServiceManager", "authenticator stop requested session_id={}", session_id);
    // Phase 1.1: Basic logging only
    // Phase 2+: Will integrate with systemd via D-Bus
    return true;
}
