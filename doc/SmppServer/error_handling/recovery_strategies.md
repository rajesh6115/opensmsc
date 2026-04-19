# Failure Recovery Strategies

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: How to handle and recover from failures  

---

## Categories of Failures

### 1. Client Errors (User Input)
- Invalid credentials
- Already bound
- Malformed PDU
- **Recovery**: Return error response, keep connection alive

### 2. System Errors (Internal)
- D-Bus service unavailable
- File I/O failure
- Memory allocation failure
- **Recovery**: Log, return error response, possibly close connection

### 3. Network Errors (I/O)
- Client disconnect
- Socket read/write timeout
- Connection reset
- **Recovery**: Close connection, log, cleanup session

---

## Recovery by Component

### SmppConnection

#### Scenario: Client sends invalid PDU

```cpp
void SmppConnection::on_body_read(...) {
    try {
        SmppMessage msg = SmppMessage::decode(full_pdu);
        // ...
    } catch (const InvalidPduException& ex) {
        logger_->warn("Invalid PDU: {}", ex.what());
        // Recovery: Close this connection (PDU corruption likely)
        on_disconnect("Invalid PDU");
        return;
    }
}
```

**Recovery Action**: Close connection (don't trust stream integrity)

---

#### Scenario: Socket read timeout

```cpp
void SmppConnection::on_header_read(const asio::error_code& ec, ...) {
    if (ec == asio::error::timed_out) {
        logger_->warn("Read timeout from {}", session_->client_ip());
        on_disconnect("Read timeout");
        return;
    }
    if (ec) {
        logger_->error("Socket error: {}", ec.message());
        on_disconnect("Socket error");
        return;
    }
    // ...
}
```

**Recovery Action**: Close connection (timeout likely indicates dead client)

---

#### Scenario: Write to client fails

```cpp
void SmppConnection::on_write(const asio::error_code& ec, size_t bytes_written) {
    if (ec) {
        logger_->warn("Write failed to {}: {}", 
                     session_->client_ip(), ec.message());
        on_disconnect("Write error");
        return;
    }
    // Continue reading next PDU
    read_pdu_header();
}
```

**Recovery Action**: Close connection (client likely gone)

---

### TcpServer

#### Scenario: IP whitelist file missing

```cpp
void TcpServer::start() {
    try {
        ip_validator_->load_whitelist(WHITELIST_FILE);
    } catch (const std::exception& ex) {
        logger_->error("Failed to load whitelist: {}", ex.what());
        // Recovery: Continue with empty whitelist (reject all IPs)
        logger_->warn("Starting with empty IP whitelist - all connections rejected");
    }
    
    // Continue accepting (but will reject all because whitelist is empty)
    restart_accept();
}
```

**Recovery Action**: Continue with safe default (deny all)

---

#### Scenario: Accept fails repeatedly

```cpp
void TcpServer::on_accept(const asio::error_code& ec, asio::ip::tcp::socket socket) {
    if (ec) {
        if (ec == asio::error::too_many_open_files) {
            logger_->error("Too many open files - rejecting connection");
            // Recovery: Don't accept more, wait for some to close
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            logger_->error("Accept error: {}", ec.message());
        }
        
        // Try to continue accepting
        restart_accept();
        return;
    }
    // ...
}
```

**Recovery Action**: Throttle acceptance, try again

---

### SmppMessageProcessor

#### Scenario: Handler throws exception

```cpp
SmppMessage SmppMessageProcessor::process_message(
    const SmppMessage& request,
    SmppSession& session) {
    
    try {
        switch (request.command_id()) {
        case BIND_TRANSMITTER:
            return bind_tx_handler_->handle(request, session);
        default:
            return SmppMessageEncoder::build_generic_resp(
                request.command_id(),
                ESME_RINVCMDID,
                request.sequence_number());
        }
    } catch (const std::exception& ex) {
        logger_->error("Handler exception: {}", ex.what());
        // Recovery: Return error response, keep connection alive
        return SmppMessageEncoder::build_generic_resp(
            request.command_id(),
            ESME_RSUBMITFAIL,  // Generic error
            request.sequence_number());
    }
}
```

**Recovery Action**: Return error response (don't close connection)

---

### BindTransmitterHandler

#### Scenario: D-Bus service unavailable

```cpp
SmppMessage BindTransmitterHandler::handle(
    const SmppMessage& request,
    SmppSession& session) {
    
    try {
        auto auth = dbus_auth_->authenticate(username, password, client_ip);
        if (!auth.success) {
            return SmppMessageEncoder::build_bind_transmitter_resp(
                ESME_RINVPASWD,
                request.sequence_number());
        }
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_ROK,
            request.sequence_number());
            
    } catch (const DBusException& ex) {
        logger_->error("D-Bus error during BIND: {}", ex.what());
        // Recovery: Return service error
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_RSUBMITFAIL,
            request.sequence_number());
    }
}
```

**Recovery Action**: Return error response, let client retry

---

### DBusAuthenticator

#### Scenario: D-Bus connection lost

```cpp
AuthResult DBusAuthenticator::authenticate(
    const std::string& username,
    const std::string& password,
    const std::string& client_ip) const {
    
    try {
        ensure_connected();  // Try to reconnect if lost
        
        // D-Bus call
        auto method = connection_->createMethodCall(...);
        auto reply = connection_->callMethod(method);
        
        bool success;
        std::string message;
        reply >> success >> message;
        
        return { success, message };
        
    } catch (const std::exception& ex) {
        // Recovery: Return auth failure (safe default)
        logger_->warn("D-Bus error: {}", ex.what());
        return { false, "D-Bus service unavailable" };
    }
}

void DBusAuthenticator::ensure_connected() const {
    std::lock_guard<std::shared_mutex> lock(connection_mutex_);
    
    if (connection_) {
        try {
            // Test connection with ping
            connection_->getConnection().getInterfaceFlags();
            return;  // Still good
        } catch (...) {
            connection_.reset();  // Connection lost
        }
    }
    
    // Reconnect
    try {
        connection_ = sdbus::createSessionBusConnection();
    } catch (const std::exception& ex) {
        throw DBusException("Cannot reconnect to D-Bus: " + std::string(ex.what()));
    }
}
```

**Recovery Action**: Attempt reconnect, fail gracefully if not available

---

### Logger

#### Scenario: Cannot create log file

```cpp
void Logger::init(const std::string& log_file_path, const std::string& level) {
    try {
        // Try to create rotating file sink
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file_path,
            10 * 1024 * 1024,  // 10 MB
            5);                 // 5 files
        
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        std::vector<spdlog::sink_ptr> sinks { console_sink, file_sink };
        auto logger = std::make_shared<spdlog::logger>("", sinks.begin(), sinks.end());
        
        logger->set_level(level_from_string(level));
        spdlog::set_default_logger(logger);
        
    } catch (const std::exception& ex) {
        // Recovery: Fallback to stdout only
        spdlog::stderr_color_mt("console");
        spdlog::warn("Cannot create file sink at {}: {} - using stdout only",
                    log_file_path, ex.what());
    }
}
```

**Recovery Action**: Graceful degradation to stdout

---

## Session Cleanup

### On Client Disconnect

```cpp
void SmppConnection::on_disconnect(const std::string& reason) {
    try {
        // Log the disconnect
        logger_->info("Client {} disconnected: {}",
                     session_->session_id(), reason);
        
        // Remove from active sessions
        session_manager_->remove_session(session_->session_id());
        
        // Close socket
        if (socket_.is_open()) {
            asio::error_code ec;
            socket_.close(ec);  // Ignore errors
        }
        
    } catch (const std::exception& ex) {
        logger_->error("Error during disconnect: {}", ex.what());
        // Continue cleanup despite error
    }
}
```

**What Happens**:
1. Session removed from manager
2. SmppSession object destroyed
3. Socket closed
4. TcpServer removes SmppConnection from active_connections_ set
5. SmppConnection object destroyed

---

### Idle Session Cleanup

```cpp
// Background task (e.g., every 60 seconds)
void cleanup_idle_sessions() {
    const auto IDLE_THRESHOLD = 3600;  // 1 hour
    
    session_manager_->cleanup_idle_sessions(IDLE_THRESHOLD);
    // Removes sessions with last_activity > 1 hour ago
    
    logger_->info("Cleaned up idle sessions");
}
```

---

## Retry Strategies

### When to Retry

| Failure | Retriable? | Strategy |
|---------|-----------|----------|
| Invalid credentials | No | Return error response |
| D-Bus unavailable | Yes | Reconnect on next auth request |
| Client disconnect | No | Close connection, cleanup |
| File I/O error | Maybe | Log, use fallback (stdout) |

### How to Implement Retry

```cpp
// Example: D-Bus reconnect on next call
class DBusAuthenticator {
private:
    mutable std::unique_ptr<sdbus::IConnection> connection_;
    
public:
    void ensure_connected() const {
        // If connection is null or broken, reconnect
        if (!connection_) {
            try {
                connection_ = sdbus::createSessionBusConnection();
            } catch (...) {
                // Retry once
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                connection_ = sdbus::createSessionBusConnection();
            }
        }
    }
};
```

---

## Graceful Degradation

### Principle

If a non-critical system fails, continue with reduced capability rather than crashing.

### Example: Missing Whitelist File

```
Ideal scenario:
  Load whitelist from /etc/smpp/whitelist.txt
  Accept connections from listed IPs

Failed scenario (missing file):
  Log warning
  Use empty whitelist
  Reject ALL connections (safe default)
  ← Server still runs, but no clients can connect
```

### Example: D-Bus Unavailable

```
Ideal scenario:
  Validate credentials via SMPPAuthenticator service

Failed scenario (D-Bus down):
  Log error
  Return auth failure
  ← Client rejects BIND, but server is still up
  ← When D-Bus comes back, next BIND attempt succeeds
```

---

## Monitoring Recovery

### Health Checks

Log indicators of recovery:

```cpp
// After successful reconnect
logger_->info("D-Bus connection re-established");

// After falling back to degraded mode
logger_->warn("Operating in degraded mode: file logging unavailable");

// After cleaning up idle sessions
logger_->info("Cleaned up {} idle sessions", count);
```

### Alerting

For operational monitoring:

```cpp
// Critical: Log to CRITICAL level if system cannot function
if (ip_validator_->is_empty()) {
    logger_->critical("IP whitelist is empty - all connections will be rejected");
}

// Warning: Log to WARN level if operating in degraded mode
if (!file_sink_available) {
    logger_->warn("File logging unavailable, using stdout only");
}
```

---

## Testing Recovery Paths

### Unit Test: Handler Catches Exception

```cpp
TEST_CASE("Handler catches D-Bus exception") {
    auto mock_auth = std::make_shared<MockDBusAuthenticator>();
    mock_auth->set_throw_exception(true);
    
    auto handler = std::make_shared<BindTransmitterHandler>(
        mock_auth, logger);
    
    SmppMessage request(...);
    SmppSession session("s1", "127.0.0.1");
    
    auto response = handler->handle(request, session);
    
    // Should return error response, not throw
    REQUIRE(response.command_status() == ESME_RSUBMITFAIL);
    REQUIRE(!session.is_bound());
}
```

### Integration Test: Connection Closes on Invalid PDU

```cpp
TEST_CASE("SmppConnection closes on invalid PDU") {
    // Create connection with mock socket
    // Feed invalid PDU data
    // Verify connection closes
}
```

---

**Summary of Recovery Strategies**:

| Layer | Failure | Recovery |
|-------|---------|----------|
| Presentation | Socket error | Close connection |
| Presentation | Read timeout | Close connection |
| Business Logic | Auth fails | Return error response |
| Business Logic | Invalid PDU | Close connection |
| Infrastructure | D-Bus unavailable | Return error, retry next call |
| Infrastructure | File I/O fails | Use fallback (stdout) |
| Infrastructure | Whitelist missing | Empty whitelist (deny all) |

---

**End of LLD Documentation**
