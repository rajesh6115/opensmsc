#include "smpp_service_manager.hpp"

#include <iostream>
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
    std::cout << "[INFO] SmppServiceManager: initialized (Phase 1.1 MVP)\n";
}

SmppServiceManager::~SmppServiceManager() = default;

// ── Public API ────────────────────────────────────────────────────────────────

bool SmppServiceManager::start_authenticator(const std::string& session_id,
                                              const std::string& client_ip)
{
    std::cout << "[INFO] SmppServiceManager: authenticator start requested\n"
              << "       session_id=" << session_id
              << "  client_ip=" << client_ip << "\n";
    // Phase 1.1: Basic logging only
    // Phase 2+: Will integrate with systemd via D-Bus
    return true;
}

bool SmppServiceManager::stop_authenticator(const std::string& session_id)
{
    std::cout << "[INFO] SmppServiceManager: authenticator stop requested\n"
              << "       session_id=" << session_id << "\n";
    // Phase 1.1: Basic logging only
    // Phase 2+: Will integrate with systemd via D-Bus
    return true;
}
