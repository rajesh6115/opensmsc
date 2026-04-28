#pragma once

#include <string>

// Builds the systemd-run command for launching a SmppClientHandler process.
// Exported as a free function so it can be unit-tested without spawning.
std::string build_launch_command(const std::string& uuid, const std::string& client_ip);

// Launches SmppClientHandler as a transient systemd unit.
// Returns true on success (exit code 0 from systemd-run).
bool launch_client_handler(const std::string& uuid, const std::string& client_ip);
