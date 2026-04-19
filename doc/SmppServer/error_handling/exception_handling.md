# Exception Handling Strategy

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: When and how to throw/catch exceptions  

---

## Exception Hierarchy

Define in `SMPPServer/include/exceptions.hpp`:

```cpp
#ifndef SMPPSERVER_EXCEPTIONS_HPP_
#define SMPPSERVER_EXCEPTIONS_HPP_

#include <stdexcept>

namespace smpp {

// Base exception
class SmppException : public std::runtime_error {
public:
    explicit SmppException(const std::string& msg)
        : std::runtime_error(msg) {}
};

// Specific exceptions
class InvalidPduException : public SmppException {
public:
    explicit InvalidPduException(const std::string& msg)
        : SmppException("Invalid PDU: " + msg) {}
};

class AuthenticationException : public SmppException {
public:
    explicit AuthenticationException(const std::string& msg)
        : SmppException("Authentication failed: " + msg) {}
};

class DBusException : public SmppException {
public:
    explicit DBusException(const std::string& msg)
        : SmppException("D-Bus error: " + msg) {}
};

class SessionException : public SmppException {
public:
    explicit SessionException(const std::string& msg)
        : SmppException("Session error: " + msg) {}
};

}  // namespace smpp

#endif  // SMPPSERVER_EXCEPTIONS_HPP_
```

---

## When to Throw Exceptions

### 1. Programming Errors (Constructor Validation)

**Rule**: Throw if precondition violated (invariant broken).

```cpp
class SmppConnection {
public:
    SmppConnection(
        asio::ip::tcp::socket socket,
        std::shared_ptr<SmppSession> session,
        std::shared_ptr<SmppMessageProcessor> processor,
        std::shared_ptr<Logger> logger)
        : socket_(std::move(socket)),
          session_(session),
          processor_(processor),
          logger_(logger) {
        
        // Throw for null dependencies (programming error)
        if (!session || !processor || !logger) {
            throw std::invalid_argument("null dependency");
        }
    }
};
```

### 2. Unrecoverable Errors (Fatal Conditions)

**Rule**: Throw if system cannot continue operating.

```cpp
class Logger {
public:
    static void init(const std::string& path, const std::string& level) {
        try {
            // Try to create file sink
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                path, 10*1024*1024, 5);
        } catch (const std::exception& ex) {
            // File sink creation failed
            // Try fallback: stdout only
            spdlog::stderr_color_mt("console");
            spdlog::warn("File sink failed, using stdout only: {}", ex.what());
            // Don't throw - graceful degradation
        }
    }
};
```

### 3. Invalid Protocol Data

**Rule**: Throw for malformed PDUs (validation failure).

```cpp
class SmppMessage {
public:
    static SmppMessage decode(const std::vector<uint8_t>& bytes) {
        if (bytes.size() < 16) {
            throw InvalidPduException("PDU too short (need 16 bytes minimum)");
        }
        
        uint32_t command_length = extract_command_length(bytes);
        if (command_length != bytes.size()) {
            throw InvalidPduException(
                "PDU length mismatch: header says " + 
                std::to_string(command_length) + 
                ", actual " + std::to_string(bytes.size()));
        }
        
        return SmppMessage(...);
    }
};
```

---

## When NOT to Throw Exceptions

### 1. Expected Validation Failures

**Rule**: Return bool or error code for user input validation.

```cpp
// ❌ DON'T throw
class SmppSession {
    bool try_bind_as_transmitter(const std::string& user) {
        if (state_ != State::UNBOUND) {
            throw SessionException("Invalid state");  // ❌ Bad
        }
        return true;
    }
};

// ✓ DO return false
class SmppSession {
    bool try_bind_as_transmitter(const std::string& user) {
        if (state_ != State::UNBOUND) {
            return false;  // ✓ Good - expected case
        }
        state_ = State::BOUND_TRANSMITTER;
        return true;
    }
};
```

### 2. Client Authentication Failures

**Rule**: Return error response, not exception.

```cpp
// ❌ DON'T throw
class BindTransmitterHandler {
    SmppMessage handle(const SmppMessage& req, SmppSession& session) {
        auto auth = dbus_auth_->authenticate(user, pass, ip);
        if (!auth.success) {
            throw AuthenticationException("Invalid credentials");  // ❌ Bad
        }
        return ...;
    }
};

// ✓ DO return error response
class BindTransmitterHandler {
    SmppMessage handle(const SmppMessage& req, SmppSession& session) {
        auto auth = dbus_auth_->authenticate(user, pass, ip);
        if (!auth.success) {
            return SmppMessageEncoder::build_bind_transmitter_resp(
                ESME_RINVPASWD,
                req.sequence_number());  // ✓ Good
        }
        return ...;
    }
};
```

---

## Exception Handling by Layer

### Presentation Layer (TcpServer, SmppConnection)

```cpp
class TcpServer {
    void on_accept(const asio::error_code& ec, asio::ip::tcp::socket socket) {
        try {
            if (ec) {
                logger_->error("Accept error: {}", ec.message());
                return;  // Recoverable, continue accepting
            }
            
            // Validate IP
            if (!ip_validator_->is_whitelisted(ip)) {
                logger_->warn("IP rejected: {}", ip);
                return;  // Expected case, no exception
            }
            
            // Create SmppConnection (may throw if null dependencies)
            auto conn = std::make_shared<SmppConnection>(...);
            conn->start();
            
        } catch (const std::exception& ex) {
            logger_->error("Unexpected error in on_accept: {}", ex.what());
            // Don't rethrow - would crash io_context
        }
    }
};

class SmppConnection {
    void on_body_read(const asio::error_code& ec, size_t bytes_read) {
        try {
            if (ec) {
                on_disconnect("I/O error");
                return;  // Network error, close connection
            }
            
            // Process message (may throw for invalid PDU)
            SmppMessage msg = SmppMessage::decode(full_pdu);
            SmppMessage response = processor_->process_message(msg, *session_);
            write_pdu(response.encode());
            
        } catch (const InvalidPduException& ex) {
            logger_->warn("Invalid PDU: {}", ex.what());
            on_disconnect("Invalid PDU");  // Malformed data, close
            
        } catch (const std::exception& ex) {
            logger_->error("Unexpected error processing message: {}", ex.what());
            on_disconnect("Processing error");
        }
    }
};
```

### Business Logic Layer (Handlers, SmppMessageProcessor)

```cpp
class SmppMessageProcessor {
    SmppMessage process_message(
        const SmppMessage& request,
        SmppSession& session) {
        
        try {
            switch (request.command_id()) {
            case BIND_TRANSMITTER:
                return bind_tx_handler_->handle(request, session);
            case UNBIND:
                return unbind_handler_->handle(request, session);
            default:
                return SmppMessageEncoder::build_generic_resp(
                    request.command_id(),
                    ESME_RINVCMDID,
                    request.sequence_number());
            }
        } catch (const SmppException& ex) {
            logger_->error("SMPP error: {}", ex.what());
            return SmppMessageEncoder::build_generic_resp(
                request.command_id(),
                ESME_RSUBMITFAIL,
                request.sequence_number());
                
        } catch (const std::exception& ex) {
            logger_->error("Unexpected error in message processor: {}", ex.what());
            return SmppMessageEncoder::build_generic_resp(
                request.command_id(),
                ESME_RSYSERR,
                request.sequence_number());
        }
    }
};

class BindTransmitterHandler {
    SmppMessage handle(const SmppMessage& request, SmppSession& session) {
        // Validation failures → return error response, NOT exception
        if (session.is_bound()) {
            return SmppMessageEncoder::build_bind_transmitter_resp(
                ESME_RALYBND,
                request.sequence_number());
        }
        
        try {
            // Protocol errors → catch and return error
            auto username = request.get_system_id();
            if (username.empty()) {
                return SmppMessageEncoder::build_bind_transmitter_resp(
                    ESME_RINVCMDLEN,
                    request.sequence_number());
            }
            
            // D-Bus call (may fail)
            auto auth = dbus_auth_->authenticate(username, password, ip);
            if (!auth.success) {
                return SmppMessageEncoder::build_bind_transmitter_resp(
                    ESME_RINVPASWD,
                    request.sequence_number());
            }
            
            return SmppMessageEncoder::build_bind_transmitter_resp(
                ESME_ROK,
                request.sequence_number());
                
        } catch (const DBusException& ex) {
            logger_->error("D-Bus error in BIND: {}", ex.what());
            return SmppMessageEncoder::build_bind_transmitter_resp(
                ESME_RSUBMITFAIL,
                request.sequence_number());
        }
    }
};
```

### Infrastructure Layer (Validators, Authenticator)

```cpp
class DBusAuthenticator {
    AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        const std::string& client_ip) const {
        
        try {
            ensure_connected();  // May throw
            
            // D-Bus call
            auto reply = connection_->callMethod(...);
            bool success;
            std::string message;
            reply >> success >> message;
            
            return { success, message };
            
        } catch (const std::exception& ex) {
            // D-Bus service unavailable or error
            // Return error result, don't throw (let caller handle)
            return { false, "D-Bus error: " + std::string(ex.what()) };
        }
    }
};

class IpValidator {
    void load_whitelist(const std::string& file_path) {
        try {
            std::ifstream file(file_path);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot open file: " + file_path);
            }
            // Parse file
        } catch (const std::exception& ex) {
            // Log error, but don't crash
            logger_->warn("Failed to load whitelist: {}", ex.what());
            // Whitelist remains empty (all IPs rejected)
        }
    }
};
```

---

## Exception Safety

### No Exception in Destructors

```cpp
class SmppSession {
    ~SmppSession() {
        try {
            // Cleanup
            cleanup();
        } catch (...) {
            // Swallow - never throw from destructor
        }
    }
};
```

### No Exception in ASIO Callbacks

```cpp
// ❌ DON'T let exception escape ASIO callback
void on_header_read_bad(const asio::error_code& ec, size_t bytes) {
    throw std::runtime_error("Error!");  // ❌ Crashes io_context!
}

// ✓ DO catch all exceptions in callbacks
void on_header_read_good(const asio::error_code& ec, size_t bytes) {
    try {
        // Processing
    } catch (...) {
        // Log and handle
    }
}
```

---

## Testing Exception Paths

### Test Successful Case

```cpp
TEST_CASE("Handler succeeds with valid credentials") {
    // ... should not throw
}
```

### Test Error Cases

```cpp
TEST_CASE("Handler returns error response for invalid credentials") {
    // ... should return ESME_RINVPASWD, not throw
}

TEST_CASE("Handler catches D-Bus exception") {
    auto mock_dbus = std::make_shared<MockDBusAuthenticator>();
    mock_dbus->set_throw_exception(true);
    
    // ... handler should catch and return error response
}

TEST_CASE("Constructor throws on null dependency") {
    REQUIRE_THROWS_AS(
        SmppConnection(socket, nullptr, processor, logger),
        std::invalid_argument);
}
```

---

**Next**: recovery_strategies.md
