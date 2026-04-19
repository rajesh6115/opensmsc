# Common Patterns & Coding Conventions

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: Design patterns and conventions used throughout SMPPServer  

---

## Dependency Injection Pattern

All classes receive dependencies via constructor, never create them internally.

### Why
- Testability: can inject mocks
- Loose coupling: classes don't know how to create dependencies
- Flexibility: can swap implementations

### Pattern

```cpp
// SmppConnection receives all dependencies via constructor
class SmppConnection {
public:
    SmppConnection(
        asio::ip::tcp::socket socket,
        std::shared_ptr<SmppSession> session,
        std::shared_ptr<SmppMessageProcessor> processor,
        std::shared_ptr<SessionManager> session_manager,
        std::shared_ptr<Logger> logger)
        : socket_(std::move(socket)),
          session_(session),
          processor_(processor),
          session_manager_(session_manager),
          logger_(logger) {
        
        if (!session || !processor || !session_manager || !logger) {
            throw std::invalid_argument("null dependency");
        }
    }
    
private:
    asio::ip::tcp::socket socket_;
    std::shared_ptr<SmppSession> session_;
    std::shared_ptr<SmppMessageProcessor> processor_;
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<Logger> logger_;
};

// SmppServer creates all dependencies and passes them
void SmppServer::start() {
    auto logger = std::make_shared<Logger>();
    auto ip_validator = std::make_shared<IpValidator>();
    auto session_manager = std::make_shared<SessionManager>();
    auto processor = std::make_shared<SmppMessageProcessor>(
        cred_validator, dbus_auth, logger);
    
    auto tcp_server = std::make_shared<TcpServer>(
        io_context_,
        port,
        ip_validator,
        processor,
        session_manager,
        logger);
    
    tcp_server->start();
}
```

### Anti-Pattern (DON'T DO)

```cpp
// ❌ BAD: Creating dependencies internally
class SmppConnection {
public:
    SmppConnection(asio::ip::tcp::socket socket) {
        logger_ = std::make_shared<Logger>();  // ❌ Bad!
        session_ = std::make_shared<SmppSession>(...);  // ❌ Bad!
    }
};

// ❌ BAD: Global dependencies
class SmppConnection {
    Logger* logger = global_logger_instance;  // ❌ Bad!
};
```

---

## State Machine Pattern

SmppSession enforces valid state transitions via try_* methods.

### Pattern

```cpp
class SmppSession {
public:
    enum class State {
        UNBOUND,
        BOUND_TRANSMITTER,
        BOUND_RECEIVER,
        BOUND_TRANSCEIVER,
        CLOSED
    };
    
    // State queries
    bool is_bound() const { return state_ != State::UNBOUND; }
    bool can_transmit() const { return state_ == State::BOUND_TRANSMITTER || 
                                      state_ == State::BOUND_TRANSCEIVER; }
    
    // State transitions (return false if invalid)
    bool try_bind_as_transmitter(const std::string& username) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_ != State::UNBOUND) {
            return false;  // Invalid transition
        }
        
        state_ = State::BOUND_TRANSMITTER;
        authenticated_as_ = username;
        return true;
    }
    
    bool try_unbind() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_ == State::UNBOUND || state_ == State::CLOSED) {
            return false;  // Already unbound or closed
        }
        
        state_ = State::UNBOUND;
        return true;
    }
    
private:
    State state_;
    std::string authenticated_as_;
    mutable std::mutex state_mutex_;
};

// Usage
SmppSession session("sess_1", "127.0.0.1");

if (session.try_bind_as_transmitter("user")) {
    // State changed successfully
} else {
    // Invalid transition, no state change
    spdlog::warn("Cannot bind: not in UNBOUND state");
}
```

### Rules
- State is always locked when checked and modified (no TOCTOU issues)
- try_* methods return bool: true if transition succeeded, false if invalid
- Each try_* method validates current state before changing
- Invalid transitions are silently rejected (no exceptions, just return false)

---

## Strategy Pattern (Handlers)

Each handler implements SmppRequestHandler interface. SmppMessageProcessor routes to appropriate handler.

### Pattern

```cpp
// Interface
class SmppRequestHandler {
public:
    virtual ~SmppRequestHandler() = default;
    virtual SmppMessage handle(
        const SmppMessage& request,
        SmppSession& session) = 0;
};

// Concrete handlers
class BindTransmitterHandler : public SmppRequestHandler {
public:
    BindTransmitterHandler(
        std::shared_ptr<DBusAuthenticator> auth,
        std::shared_ptr<Logger> logger)
        : auth_(auth), logger_(logger) {}
    
    SmppMessage handle(
        const SmppMessage& request,
        SmppSession& session) override {
        // BIND_TRANSMITTER logic
    }
};

class UnbindHandler : public SmppRequestHandler {
public:
    UnbindHandler(std::shared_ptr<Logger> logger)
        : logger_(logger) {}
    
    SmppMessage handle(
        const SmppMessage& request,
        SmppSession& session) override {
        // UNBIND logic
    }
};

// Router (SmppMessageProcessor)
class SmppMessageProcessor {
private:
    std::shared_ptr<SmppRequestHandler> bind_tx_handler_;
    std::shared_ptr<SmppRequestHandler> bind_rx_handler_;
    std::shared_ptr<SmppRequestHandler> unbind_handler_;
    // ... more handlers
    
public:
    SmppMessage process_message(
        const SmppMessage& request,
        SmppSession& session) {
        
        switch (request.command_id()) {
        case BIND_TRANSMITTER:
            return bind_tx_handler_->handle(request, session);
        case BIND_RECEIVER:
            return bind_rx_handler_->handle(request, session);
        case UNBIND:
            return unbind_handler_->handle(request, session);
        default:
            return build_error_response(...);
        }
    }
};
```

### Rules
- All handlers are stateless (no member state except dependencies)
- Handlers can be reused for multiple clients
- Handlers throw exceptions on fatal errors, return error responses for validation failures
- Each handler owns one responsibility (one command type)

---

## Value Object Pattern

SmppMessage is immutable. Once created, cannot be modified.

### Pattern

```cpp
class SmppMessage {
public:
    // Constructor (only way to create)
    SmppMessage(
        uint32_t command_id,
        uint32_t command_status,
        uint32_t sequence_number,
        const std::vector<uint8_t>& body = {});
    
    // Accessors (all const, return by const reference)
    uint32_t command_id() const { return command_id_; }
    uint32_t command_status() const { return command_status_; }
    const std::vector<uint8_t>& body() const { return body_; }
    
    // NO setters - this is intentional!
    
    // Only modifications are via factory/decode methods
    static SmppMessage decode(const std::vector<uint8_t>& bytes);
    
private:
    // All members are const in spirit (never modified after construction)
    const uint32_t command_id_;
    const uint32_t command_status_;
    const uint32_t sequence_number_;
    const std::vector<uint8_t> body_;
};

// Usage
SmppMessage msg1(BIND_TRANSMITTER, ESME_ROK, 1, body);
SmppMessage msg2 = msg1;  // Safe: copy of immutable object

// Can pass by const reference safely
void process_message(const SmppMessage& msg) {
    // msg is guaranteed immutable, no synchronization needed
}
```

### Benefits
- Thread-safe: immutable objects need no locks
- Copyable: can pass around without worrying about mutation
- Composable: can build larger structures from small value objects

---

## ASIO Async Chain Pattern

Async operations chain via callbacks. Each callback schedules the next operation.

### Pattern

```cpp
class SmppConnection {
private:
    asio::ip::tcp::socket socket_;
    std::vector<uint8_t> header_buffer_;
    std::vector<uint8_t> body_buffer_;
    
public:
    void start() {
        read_pdu_header();  // Begin the chain
    }
    
private:
    // Step 1: Read 16-byte header
    void read_pdu_header() {
        asio::async_read(
            socket_,
            asio::buffer(header_buffer_),
            [self = shared_from_this()](
                const asio::error_code& ec,
                size_t bytes_read) {
                self->on_header_read(ec, bytes_read);
            });
    }
    
    // Step 1 callback
    void on_header_read(const asio::error_code& ec, size_t bytes_read) {
        if (ec) {
            on_disconnect("header read error");
            return;
        }
        
        // Extract body length from header
        uint32_t body_len = extract_body_length(header_buffer_);
        
        // Schedule Step 2
        read_pdu_body(body_len);
    }
    
    // Step 2: Read body
    void read_pdu_body(uint32_t body_length) {
        body_buffer_.resize(body_length);
        asio::async_read(
            socket_,
            asio::buffer(body_buffer_),
            [self = shared_from_this()](
                const asio::error_code& ec,
                size_t bytes_read) {
                self->on_body_read(ec, bytes_read);
            });
    }
    
    // Step 2 callback
    void on_body_read(const asio::error_code& ec, size_t bytes_read) {
        if (ec) {
            on_disconnect("body read error");
            return;
        }
        
        // Process message
        SmppMessage msg = SmppMessage::decode(full_pdu);
        SmppMessage response = processor_->process_message(msg, *session_);
        
        // Schedule Step 3
        write_pdu(response.encode());
    }
    
    // Step 3: Write response
    void write_pdu(const std::vector<uint8_t>& pdu_bytes) {
        asio::async_write(
            socket_,
            asio::buffer(pdu_bytes),
            [self = shared_from_this()](
                const asio::error_code& ec,
                size_t bytes_written) {
                self->on_write(ec, bytes_written);
            });
    }
    
    // Step 3 callback
    void on_write(const asio::error_code& ec, size_t bytes_written) {
        if (ec) {
            on_disconnect("write error");
            return;
        }
        
        // Continue chain: read next PDU
        read_pdu_header();
    }
    
    void on_disconnect(const std::string& reason) {
        socket_.close();
        // Cleanup
    }
};
```

### Key Points
- Each async operation has a corresponding callback
- Callback schedules next operation (or ends chain)
- Use `shared_from_this()` to keep connection alive while async operation pending
- Always check error_code in callbacks
- Keep async operations short (no blocking I/O in callbacks)

---

## Error Handling Pattern

### Validation failures → Return error response
### Precondition violations → Return false
### Fatal errors → Throw exception

```cpp
// Validation failure: return error response
SmppMessage handler.handle(...) {
    auto username = request.get_system_id();
    if (username.empty()) {
        spdlog::warn("Empty username in BIND");
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_RINVPASWD,
            request.sequence_number());
    }
    // ... continue
}

// Precondition violation: return false
bool SmppSession::try_bind_as_transmitter(const std::string& user) {
    if (state_ != State::UNBOUND) {
        return false;  // Invalid state, no state change
    }
    // ... continue
}

// Fatal/programming error: throw exception
BindTransmitterHandler::BindTransmitterHandler(
    std::shared_ptr<DBusAuthenticator> auth,
    std::shared_ptr<Logger> logger)
    : auth_(auth), logger_(logger) {
    
    if (!auth || !logger) {
        throw std::invalid_argument("null dependency");  // ← throw
    }
}

// I/O error in async callback: log and close connection
void SmppConnection::on_header_read(..., size_t bytes_read) {
    if (ec) {
        logger_->error("Header read error: {}", ec.message());
        on_disconnect("I/O error");
        return;
    }
    // ... continue
}
```

### Rules
- Input validation failures (malformed PDU, invalid credentials) → return error response (bool or SmppMessage with error code)
- Precondition violations (state machine invalid transition) → return false (no exception)
- Programming errors (null dependencies, assertion failures) → throw exception
- I/O errors in async callbacks → log and close connection (don't throw, exceptions in callbacks crash io_context)

---

## Thread Safety Pattern

Use `std::shared_lock` for readers, `std::lock_guard` for writers.

```cpp
class SmppSession {
private:
    State state_;
    mutable std::shared_mutex state_mutex_;
    
public:
    // Query (const) - use shared_lock
    bool is_bound() const {
        std::shared_lock<std::shared_mutex> lock(state_mutex_);
        return state_ != State::UNBOUND;
    }
    
    // Modification (non-const) - use lock_guard
    bool try_bind_as_transmitter(const std::string& user) {
        std::lock_guard<std::shared_mutex> lock(state_mutex_);
        
        if (state_ != State::UNBOUND) {
            return false;
        }
        
        state_ = State::BOUND_TRANSMITTER;
        return true;
    }
};
```

### Rules
- Mutable state is protected by mutex
- Lock is acquired at method entry, released on scope exit
- Use `std::shared_lock` for const methods (multiple readers)
- Use `std::lock_guard` for non-const methods (exclusive writer)
- Never hold lock across I/O operations (D-Bus calls, socket writes, etc.)

---

## Logging Pattern

Log at appropriate levels. Embed module name in message via macro.

```cpp
// In code
LOG_INFO("SmppConnection", "Client connected from {}", client_ip);
LOG_WARN("IpValidator", "IP rejected: {}", ip);
LOG_ERROR("DBusAuthenticator", "D-Bus call failed: {}", error);
LOG_FATAL("SmppServer", "Fatal error, exiting");

// In output
2026-04-19 05:01:23.456 [INFO ] [SmppConnection       ] Client connected from 127.0.0.1
2026-04-19 05:01:23.457 [WARN ] [IpValidator          ] IP rejected: 192.168.1.50
2026-04-19 05:01:23.458 [ERROR] [DBusAuthenticator    ] D-Bus call failed: timeout
2026-04-19 05:01:23.459 [CRIT ] [SmppServer           ] Fatal error, exiting
```

### Log Levels
- **DEBUG**: Low-level details (entering/exiting functions, variable values) — only for troubleshooting
- **INFO**: Normal events (server starting, client connecting, BIND successful) — informational
- **WARN**: Invalid but recovered (rejected IP, invalid credentials, retry attempt) — user-recoverable errors
- **ERROR**: Unexpected errors (I/O failures, exceptions, invalid state) — problems to investigate
- **CRITICAL/FATAL**: System shutting down — immediate attention needed

### Macro Usage
```cpp
#define LOG_INFO(module, fmt, ...) \
    spdlog::info("[{:<20}] " fmt, module, ##__VA_ARGS__)

// Usage examples
LOG_INFO("TcpServer", "Listening on port {}", 2775);
LOG_WARN("CredentialValidator", "Invalid username: {} (too long)", username);
LOG_ERROR("SmppConnection", "PDU decode failed, closing connection");
```

---

## Smart Pointer Usage

Use `std::shared_ptr` for shared ownership, `std::make_shared` for creation.

```cpp
// ✓ Good: use make_shared
auto logger = std::make_shared<Logger>();

// ✓ Good: pass to dependencies
auto server = std::make_shared<TcpServer>(
    io_context_,
    port,
    ip_validator,
    logger);

// ✓ Good: store and reference
std::shared_ptr<SmppSession> session = session_manager_->create_session(ip);

// ❌ Bad: raw pointer in long-lived context
SmppSession* session = new SmppSession(...);  // Memory leak risk!

// ❌ Bad: raw new/delete
delete session;
```

### Rules
- Use `std::shared_ptr` for objects that need shared lifetime
- Use `std::make_shared` (more efficient, exception-safe)
- Never use raw `new`/`delete` in application code
- Move dependencies in constructors: `std::move(dep)`

---

**Next**: testing strategy documents
