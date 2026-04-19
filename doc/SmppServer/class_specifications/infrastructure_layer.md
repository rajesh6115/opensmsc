# Infrastructure Layer: Class Specifications

**Date**: 2026-04-19  
**Version**: 1.0  
**Layer**: Infrastructure Layer (Utilities & Services)  
**Classes**: IpValidator, CredentialValidator, DBusAuthenticator, Logger, SessionManager  

---

## Overview

The Infrastructure Layer provides utility services: validation, authentication, logging, and session tracking.

- **IpValidator**: IP whitelist enforcement
- **CredentialValidator**: Credential format validation
- **DBusAuthenticator**: D-Bus bridge to SMPPAuthenticator service
- **Logger**: spdlog wrapper with dual sinks
- **SessionManager**: Active session tracking and cleanup

---

## Class 1: IpValidator

### Purpose
Check if incoming TCP connection IP is whitelisted before accepting.

### Location
- **Header**: `SMPPServer/include/validators/ip_validator.hpp`
- **Source**: `SMPPServer/src/validators/ip_validator.cpp`

### Responsibilities

**Whitelist Management**:
- Load whitelist from config file (one IP per line)
- Parse IPv4 and IPv6 addresses
- Support CIDR notation (optional, for subnets)

**Validation**:
- Check if given IP is in whitelist
- Return bool (true if allowed, false if rejected)

### Class Signature

```cpp
class IpValidator {
public:
    IpValidator();
    
    // Whitelist management
    void load_whitelist(const std::string& file_path);
    
    void add_ip(const std::string& ip_or_cidr);
    
    // Validation
    bool is_whitelisted(const std::string& client_ip) const;
    
    // Queries
    size_t whitelist_size() const;
    bool is_empty() const;
    
private:
    std::set<std::string> whitelist_ips_;
    std::set<std::pair<std::string, std::string>> whitelist_cidrs_;  // (network, mask)
    mutable std::shared_mutex whitelist_mutex_;
    
    bool is_in_cidr(const std::string& ip, const std::string& network) const;
};
```

### Processing Flow

```
Load whitelist file (e.g., /etc/simple_smpp_server/ip_whitelist.txt):
  127.0.0.1
  ::1
  192.168.1.0/24
  2001:db8::/32

IpValidator::load_whitelist(path)
  ├─ Open file
  ├─ For each line:
  │  ├─ If contains '/' → parse CIDR, add to whitelist_cidrs_
  │  └─ Else → add to whitelist_ips_
  └─ Close file

IpValidator::is_whitelisted(ip)
  ├─ Check ip in whitelist_ips_
  │  └─ if found → return true
  │
  ├─ Check ip in whitelist_cidrs_
  │  └─ if matches any CIDR → return true
  │
  └─ return false
```

### Key Implementation Details

**Whitelist File Format**:
```
# IP Whitelist for SMPP Server
# Format: one IP or CIDR per line
# Lines starting with # are comments
# Empty lines are ignored

127.0.0.1
::1
192.168.1.0/24
10.0.0.0/8
2001:db8::/32
```

**IP Matching**:
```cpp
bool IpValidator::is_whitelisted(const std::string& client_ip) const {
    std::shared_lock<std::shared_mutex> lock(whitelist_mutex_);
    
    // Exact match
    if (whitelist_ips_.count(client_ip) > 0) {
        return true;
    }
    
    // CIDR match
    for (const auto& [network, mask] : whitelist_cidrs_) {
        if (is_in_cidr(client_ip, network)) {
            return true;
        }
    }
    
    return false;
}
```

### Dependencies
- None (standard library only)

---

## Class 2: CredentialValidator

### Purpose
Validate credential format before passing to D-Bus authenticator.

### Location
- **Header**: `SMPPServer/include/validators/credential_validator.hpp`
- **Source**: `SMPPServer/src/validators/credential_validator.cpp`

### Responsibilities

**Format Validation**:
- Check username format (non-empty, alphanumeric + underscore)
- Check password format (non-empty, at least 1 char)
- Enforce length limits (username: 1-32 chars, password: 1-32 chars)

**Extraction from SMPP Body**:
- Parse C-string fields from BIND request body
- Return (username, password) tuple or error

### Class Signature

```cpp
class CredentialValidator {
public:
    // Validation result
    struct ValidationResult {
        bool valid;
        std::string username;
        std::string password;
        std::string error_message;
    };
    
    // Validate username/password
    ValidationResult validate_credentials(
        const std::string& username,
        const std::string& password) const;
    
    // Extract from BIND PDU body
    ValidationResult extract_from_bind_body(
        const std::vector<uint8_t>& body) const;
    
private:
    static constexpr int USERNAME_MAX_LEN = 32;
    static constexpr int PASSWORD_MAX_LEN = 32;
    
    bool is_valid_username(const std::string& username) const;
    bool is_valid_password(const std::string& password) const;
};
```

### Processing Flow

```
BIND_TRANSMITTER arrives with body:
  ["testuser\0"]["mypass\0"]["...\0"][...]

CredentialValidator::extract_from_bind_body(body)
  ├─ Find first \0 → extract system_id = "testuser"
  ├─ Find second \0 → extract password = "mypass"
  ├─ Validate format
  │  ├─ username empty? → invalid
  │  ├─ username > 32 chars? → invalid
  │  ├─ username has invalid chars? → invalid
  │  └─ ... similar for password
  └─ Return { valid: bool, username, password, error }

validate_credentials(username, password)
  ├─ is_valid_username(username) → bool
  ├─ is_valid_password(password) → bool
  └─ Return { valid, error_message }
```

### Key Implementation Details

**C-String Extraction from Binary**:
```cpp
CredentialValidator::ValidationResult
CredentialValidator::extract_from_bind_body(
    const std::vector<uint8_t>& body) const {
    
    if (body.empty()) {
        return { false, "", "", "Empty body" };
    }
    
    // Find first C-string (system_id)
    const uint8_t* data_ptr = body.data();
    size_t data_len = body.size();
    
    size_t null_pos = 0;
    while (null_pos < data_len && data_ptr[null_pos] != '\0') {
        null_pos++;
    }
    
    if (null_pos >= data_len) {
        return { false, "", "", "Malformed BIND body (no system_id terminator)" };
    }
    
    std::string username(
        reinterpret_cast<const char*>(data_ptr),
        null_pos);
    
    // Move past username and null terminator
    data_ptr += null_pos + 1;
    data_len -= (null_pos + 1);
    
    // Find second C-string (password)
    null_pos = 0;
    while (null_pos < data_len && data_ptr[null_pos] != '\0') {
        null_pos++;
    }
    
    if (null_pos >= data_len) {
        return { false, "", "", "Malformed BIND body (no password terminator)" };
    }
    
    std::string password(
        reinterpret_cast<const char*>(data_ptr),
        null_pos);
    
    return validate_credentials(username, password);
}
```

**Format Validation**:
```cpp
bool CredentialValidator::is_valid_username(const std::string& u) const {
    if (u.empty() || u.length() > USERNAME_MAX_LEN) {
        return false;
    }
    
    // Allow: a-z, A-Z, 0-9, _
    return std::all_of(u.begin(), u.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

bool CredentialValidator::is_valid_password(const std::string& p) const {
    // Just check length, allow any non-null bytes
    return !p.empty() && p.length() <= PASSWORD_MAX_LEN;
}
```

### Dependencies
- None (standard library only)

---

## Class 3: DBusAuthenticator

### Purpose
Bridge to SMPPAuthenticator D-Bus service. Validate username/password over D-Bus.

### Location
- **Header**: `SMPPServer/include/authenticators/dbus_authenticator.hpp`
- **Source**: `SMPPServer/src/authenticators/dbus_authenticator.cpp`

### Responsibilities

**D-Bus Connection**:
- Connect to system bus (or session bus for testing)
- Maintain connection during server lifetime

**Authentication RPC**:
- Call SMPPAuthenticator.Authenticate(username, password, client_ip)
- Return (success: bool, message: string)
- Handle D-Bus errors (service not available, timeout, etc.)

**Connection Retry**:
- If D-Bus connection lost, attempt reconnect on next auth request
- Fail gracefully if service unavailable (auth denied)

### Class Signature

```cpp
class DBusAuthenticator {
public:
    struct AuthResult {
        bool success;
        std::string message;
    };
    
    DBusAuthenticator(
        const std::string& dbus_service_name,
        bool use_session_bus = false);
    
    // Authentication
    AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        const std::string& client_ip) const;
    
    // Connection status
    bool is_connected() const;
    void reconnect();
    
private:
    std::string dbus_service_name_;
    bool use_session_bus_;
    mutable std::unique_ptr<sdbus::IConnection> connection_;
    mutable std::shared_mutex connection_mutex_;
    
    void ensure_connected() const;
};
```

### Processing Flow

```
DBusAuthenticator::authenticate(username, password, client_ip)
  ├─ ensure_connected()
  │  ├─ if (connection_ == nullptr)
  │  │  └─ Create sdbus IConnection (system or session bus)
  │  └─ if (connection error) → return AuthResult { false, "D-Bus unavailable" }
  │
  ├─ Call D-Bus method:
  │  D-Bus call: /com/telecom/SMPPAuthenticator
  │              .Authenticate(username, password, client_ip)
  │
  ├─ Wait for response (timeout: 5 seconds)
  │
  ├─ Parse response: (success: bool, message: string)
  │
  └─ return AuthResult { success, message }

D-Bus Interface (SMPPAuthenticator service):
  Method: Authenticate
    Input:
      - username (string)
      - password (string)
      - client_ip (string)
    Output:
      - success (boolean)
      - message (string)
```

### Key Implementation Details

**Connection Management**:
```cpp
void DBusAuthenticator::ensure_connected() const {
    std::lock_guard<std::shared_mutex> lock(connection_mutex_);
    
    if (connection_) {
        try {
            // Ping to check if still connected
            connection_->getConnection().getInterfaceFlags();
            return;  // Still connected
        } catch (...) {
            connection_.reset();  // Force reconnect
        }
    }
    
    try {
        if (use_session_bus_) {
            connection_ = sdbus::createSystemBusConnection(
                dbus_service_name_);
        } else {
            connection_ = sdbus::createSessionBusConnection();
        }
    } catch (const std::exception& ex) {
        // D-Bus service unavailable
        throw std::runtime_error(
            "D-Bus connection failed: " + std::string(ex.what()));
    }
}

DBusAuthenticator::AuthResult DBusAuthenticator::authenticate(
    const std::string& username,
    const std::string& password,
    const std::string& client_ip) const {
    
    try {
        ensure_connected();
        
        // Call D-Bus method
        auto method = connection_->createMethodCall(
            dbus_service_name_,
            "/com/telecom/SMPPAuthenticator",
            "com.telecom.SMPPAuthenticator",
            "Authenticate");
        
        method << username << password << client_ip;
        
        auto reply = connection_->callMethod(method);
        
        bool success = false;
        std::string message;
        reply >> success >> message;
        
        return { success, message };
        
    } catch (const std::exception& ex) {
        return { false, "D-Bus error: " + std::string(ex.what()) };
    }
}
```

### Dependencies
- sdbus-c++ (D-Bus C++ bindings)

---

## Class 4: Logger

### Purpose
spdlog wrapper. Dual sinks (stdout colored + rotating file). Initialize once, use globally via macros.

### Location
- **Header**: `SMPPServer/include/logger.hpp`
- **Source**: `SMPPServer/src/logger.cpp`

### Responsibilities

**Initialization**:
- Create stdout_color_sink_mt (colored console)
- Create rotating_file_sink_mt (10 MB max, 5 files)
- Set log level (DEBUG, INFO, WARN, ERROR)
- Set format pattern with timestamps

**Logging Interface**:
- Expose LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL macros
- Support fmt-style formatting (spdlog uses fmtlib)

**Shutdown**:
- Flush all sinks
- Close file handles

### Class Signature

```cpp
namespace Logger {
    // Initialization (call once at startup)
    void init(
        const std::string& log_file_path,
        const std::string& log_level = "info");
    
    void shutdown();
    
    // Level string conversion
    spdlog::level::level_enum level_from_string(
        const std::string& level_str);
    
    // Direct logging (if not using macros)
    void log_info(const std::string& message);
    void log_warn(const std::string& message);
    void log_error(const std::string& message);
    void log_fatal(const std::string& message);
}

// Macros for use throughout code
#define LOG_INFO(module, fmt, ...) \
    spdlog::info("[{:<20}] " fmt, module, ##__VA_ARGS__)

#define LOG_WARN(module, fmt, ...) \
    spdlog::warn("[{:<20}] " fmt, module, ##__VA_ARGS__)

#define LOG_ERROR(module, fmt, ...) \
    spdlog::error("[{:<20}] " fmt, module, ##__VA_ARGS__)

#define LOG_FATAL(module, fmt, ...) \
    spdlog::critical("[{:<20}] " fmt, module, ##__VA_ARGS__)
```

### Processing Flow

```
main()
  ├─ Parse --log-level arg
  ├─ Logger::init("/var/log/simple_smpp_server/server.log", "info")
  │  ├─ Create stdout_color_sink_mt
  │  ├─ Create rotating_file_sink_mt(path, 10MB, 5 files)
  │  ├─ Create combined logger with both sinks
  │  ├─ Set level = "info"
  │  ├─ Set pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%-5l%$] %v"
  │  └─ spdlog::set_default_logger(logger)
  │
  └─ Throughout code:
     LOG_INFO("TcpServer", "Listening on port {}", 2775)
     LOG_WARN("IpValidator", "IP rejected: {}", ip)
     LOG_ERROR("DBusAuthenticator", "Connection failed: {}", error)

Log output (both stdout and file):
  2026-04-19 05:01:23.456 [INFO ] [TcpServer            ] Listening on port 2775
  2026-04-19 05:01:23.457 [WARN ] [IpValidator          ] IP rejected: 192.168.1.50
  2026-04-19 05:01:23.458 [ERROR] [DBusAuthenticator    ] Connection failed: service unavailable

shutdown()
  ├─ Flush all sinks
  └─ Drop default logger
```

### Key Implementation Details

**Dual Sinks Setup**:
```cpp
void Logger::init(
    const std::string& log_file_path,
    const std::string& log_level) {
    
    try {
        // Stdout sink (colored)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%-5l%$] %v");
        
        // File sink (rotating, 10 MB × 5)
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file_path,
            10 * 1024 * 1024,  // 10 MB per file
            5);                 // 5 files max
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%-5l%$] %v");
        
        // Combine sinks
        std::vector<spdlog::sink_ptr> sinks { console_sink, file_sink };
        auto logger = std::make_shared<spdlog::logger>("", sinks.begin(), sinks.end());
        
        // Set level
        logger->set_level(level_from_string(log_level));
        
        // Register as default
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::err);
        
    } catch (const std::exception& ex) {
        // Fallback: stdout only
        spdlog::stderr_color_mt("console");
        spdlog::warn("Failed to create file sink, using stdout only: {}", ex.what());
    }
}

void Logger::shutdown() {
    spdlog::drop_all();
    spdlog::shutdown();
}

spdlog::level::level_enum Logger::level_from_string(
    const std::string& level_str) {
    
    std::string lower = level_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "debug") return spdlog::level::debug;
    if (lower == "info") return spdlog::level::info;
    if (lower == "warn" || lower == "warning") return spdlog::level::warn;
    if (lower == "error") return spdlog::level::err;
    if (lower == "fatal" || lower == "critical") return spdlog::level::critical;
    
    return spdlog::level::info;  // default
}
```

### Dependencies
- spdlog (header-only via CMake FetchContent)

---

## Class 5: SessionManager

### Purpose
Track active sessions. Map session_id → SmppSession. Handle cleanup on disconnect.

### Location
- **Header**: `SMPPServer/include/session_manager.hpp`
- **Source**: `SMPPServer/src/session_manager.cpp`

### Responsibilities

**Session Tracking**:
- Create session (generate session_id)
- Register session in map
- Look up session by ID
- Remove session (on disconnect)

**Queries**:
- Get all active sessions
- Count active sessions
- Find sessions by state (BOUND, UNBOUND, etc.)

**Cleanup**:
- Remove stale sessions (idle timeout)
- Graceful shutdown (close all sessions)

### Class Signature

```cpp
class SessionManager {
public:
    SessionManager();
    
    // Session management
    std::shared_ptr<SmppSession> create_session(
        const std::string& client_ip);
    
    std::shared_ptr<SmppSession> get_session(
        const std::string& session_id) const;
    
    void remove_session(const std::string& session_id);
    
    // Queries
    size_t active_count() const;
    std::vector<std::shared_ptr<SmppSession>> get_all_sessions() const;
    std::vector<std::shared_ptr<SmppSession>> get_bound_sessions() const;
    
    // Cleanup
    void cleanup_idle_sessions(std::time_t idle_threshold_seconds);
    void close_all_sessions();
    
private:
    std::map<std::string, std::shared_ptr<SmppSession>> sessions_;
    mutable std::shared_mutex sessions_mutex_;
    
    std::string generate_session_id();
    
    static std::atomic<uint64_t> session_counter_;
};
```

### Processing Flow

```
TcpServer::on_accept()
  ├─ session = session_manager_->create_session(client_ip)
  │  └─ Generates session_id, creates SmppSession, stores in map
  │
  └─ Create SmppConnection(socket, session, ...)

SmppConnection::on_disconnect()
  └─ session_manager_->remove_session(session_->session_id())
     └─ Remove from map, session object destroyed

SessionManager::cleanup_idle_sessions(threshold)
  ├─ Lock sessions_ map
  ├─ For each session:
  │  ├─ if (now - last_activity > threshold)
  │  │  └─ Remove from map (auto-close connection)
  │  └─ Log "Session idle, closed"
  └─ Unlock

Periodic cleanup (background task):
  └─ Every 60 seconds: cleanup_idle_sessions(3600)  // 1 hour idle threshold
```

### Key Implementation Details

**Thread-Safe Session Tracking**:
```cpp
std::shared_ptr<SmppSession> SessionManager::create_session(
    const std::string& client_ip) {
    
    std::string session_id = generate_session_id();
    auto session = std::make_shared<SmppSession>(
        session_id,
        client_ip);
    
    {
        std::lock_guard<std::shared_mutex> lock(sessions_mutex_);
        sessions_[session_id] = session;
    }
    
    return session;
}

std::shared_ptr<SmppSession> SessionManager::get_session(
    const std::string& session_id) const {
    
    std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    return (it != sessions_.end()) ? it->second : nullptr;
}

void SessionManager::remove_session(const std::string& session_id) {
    std::lock_guard<std::shared_mutex> lock(sessions_mutex_);
    sessions_.erase(session_id);
}
```

**Idle Cleanup**:
```cpp
void SessionManager::cleanup_idle_sessions(
    std::time_t idle_threshold_seconds) {
    
    std::lock_guard<std::shared_mutex> lock(sessions_mutex_);
    
    auto now = std::time(nullptr);
    std::vector<std::string> to_remove;
    
    for (const auto& [session_id, session] : sessions_) {
        std::time_t last_activity = session->last_activity();
        
        if ((now - last_activity) > idle_threshold_seconds) {
            to_remove.push_back(session_id);
            // Note: don't log here (this is called from non-logger context)
        }
    }
    
    for (const auto& session_id : to_remove) {
        sessions_.erase(session_id);
    }
}
```

### Dependencies
- SmppSession

---

## Dependencies Summary

**IpValidator**:
- None

**CredentialValidator**:
- None

**DBusAuthenticator**:
- sdbus-c++

**Logger**:
- spdlog

**SessionManager**:
- SmppSession

---

## Testing Checklist

- [ ] IpValidator loads whitelist correctly
- [ ] IpValidator rejects non-whitelisted IPs
- [ ] IpValidator accepts whitelisted IPs
- [ ] IpValidator handles IPv6 correctly
- [ ] CredentialValidator extracts C-strings from binary
- [ ] CredentialValidator rejects invalid formats
- [ ] CredentialValidator accepts valid credentials
- [ ] DBusAuthenticator connects to D-Bus
- [ ] DBusAuthenticator calls Authenticate method
- [ ] DBusAuthenticator handles D-Bus errors
- [ ] Logger creates both sinks (stdout + file)
- [ ] Logger respects log level
- [ ] Logger format includes timestamp
- [ ] SessionManager creates and tracks sessions
- [ ] SessionManager removes sessions
- [ ] SessionManager cleanup_idle_sessions works
- [ ] SessionManager thread-safe under concurrent access

---

**Document Status**: Complete  
**Next**: implementation_guides/class_implementation_checklist.md
