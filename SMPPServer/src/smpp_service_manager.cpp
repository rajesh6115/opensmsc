#include "smpp_service_manager.hpp"

#include <sdbus-c++/sdbus-c++.h>

#include <iostream>
#include <string>

// ── Pimpl – keeps sdbus-c++ types out of the header ───────────────────────────
//
// One shared IProxy to the systemd Manager object on the system bus.
// sdbus-c++ creates and owns the D-Bus connection internally; no event-loop
// thread is required for the synchronous method calls we make here.

struct SmppServiceManager::Impl
{
    std::unique_ptr<sdbus::IProxy> proxy;

    Impl()
        : proxy{sdbus::createProxy(
              sdbus::ServiceName{"org.freedesktop.systemd1"},
              sdbus::ObjectPath{"/org/freedesktop/systemd1"})}
    {
        std::cout << "[INFO] SmppServiceManager: D-Bus proxy ready (sdbus-c++)\n";
    }

    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
};

// ── SmppServiceManager ────────────────────────────────────────────────────────

SmppServiceManager::SmppServiceManager()
    : impl_{std::make_unique<Impl>()}
{}

SmppServiceManager::~SmppServiceManager() = default;

// ── Internal helpers ──────────────────────────────────────────────────────────

// Calls org.freedesktop.systemd1.Manager.StartUnit(name, "replace").
// Returns the D-Bus job path on success; throws sdbus::Error on failure.
static sdbus::ObjectPath call_start(sdbus::IProxy& proxy,
                                    const std::string& unit_name)
{
    sdbus::ObjectPath job;
    proxy.callMethod("StartUnit")
         .onInterface("org.freedesktop.systemd1.Manager")
         .withArguments(unit_name, std::string{"replace"})
         .storeResultsTo(job);
    return job;
}

// Calls org.freedesktop.systemd1.Manager.StopUnit(name, "replace").
static sdbus::ObjectPath call_stop(sdbus::IProxy& proxy,
                                   const std::string& unit_name)
{
    sdbus::ObjectPath job;
    proxy.callMethod("StopUnit")
         .onInterface("org.freedesktop.systemd1.Manager")
         .withArguments(unit_name, std::string{"replace"})
         .storeResultsTo(job);
    return job;
}

bool SmppServiceManager::start_unit(const std::string& unit_name)
{
    try {
        auto job = call_start(*impl_->proxy, unit_name);
        std::cout << "[INFO] SmppServiceManager: StartUnit " << unit_name
                  << "  job=" << static_cast<std::string>(job) << "\n";
        return true;
    }
    catch (const sdbus::Error& e) {
        std::cerr << "[ERROR] SmppServiceManager: StartUnit " << unit_name
                  << "  [" << e.getName() << "] " << e.getMessage() << "\n";
        return false;
    }
}

bool SmppServiceManager::stop_unit(const std::string& unit_name)
{
    try {
        auto job = call_stop(*impl_->proxy, unit_name);
        std::cout << "[INFO] SmppServiceManager: StopUnit " << unit_name
                  << "  job=" << static_cast<std::string>(job) << "\n";
        return true;
    }
    catch (const sdbus::Error& e) {
        std::cerr << "[ERROR] SmppServiceManager: StopUnit " << unit_name
                  << "  [" << e.getName() << "] " << e.getMessage() << "\n";
        return false;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

bool SmppServiceManager::start_authenticator(const std::string& session_id,
                                              const std::string& client_ip)
{
    const std::string unit = "SMPPAuthenticator@" + session_id + ".service";
    std::cout << "[INFO] SmppServiceManager: starting " << unit
              << "  client=" << client_ip << "\n";
    return start_unit(unit);
}

bool SmppServiceManager::stop_authenticator(const std::string& session_id)
{
    const std::string unit = "SMPPAuthenticator@" + session_id + ".service";
    std::cout << "[INFO] SmppServiceManager: stopping " << unit << "\n";
    return stop_unit(unit);
}
