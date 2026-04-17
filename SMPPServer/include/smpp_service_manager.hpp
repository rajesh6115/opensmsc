#pragma once

#include <memory>
#include <string>

// Starts / stops systemd template units for the SMPP Authenticator.
//
// systemd unit template: SMPPAuthenticator@.service
// Instance launched:     SMPPAuthenticator@<session_id>.service
//
// The session ID is passed to the service via the systemd instance
// specifier '%i', e.g.:
//   ExecStart=/usr/local/bin/smpp_auth --session %i
//
// Uses sdbus-c++ (https://github.com/Kistler-Group/sdbus-cpp) to talk to
// systemd over D-Bus, avoiding any shell-out to systemctl.
class SmppServiceManager
{
public:
    SmppServiceManager();
    ~SmppServiceManager();

    // Starts SMPPAuthenticator@<session_id>.service.
    // Returns true on success.
    bool start_authenticator(const std::string& session_id,
                             const std::string& client_ip);

    // Stops SMPPAuthenticator@<session_id>.service.
    // Returns true on success.
    bool stop_authenticator(const std::string& session_id);

private:
    // Pimpl to isolate sdbus-c++ types from consumers of this header.
    struct Impl;
    std::unique_ptr<Impl> impl_;

    bool start_unit(const std::string& unit_name);
    bool stop_unit(const std::string& unit_name);
};
