# SMPPServer Class Design Plan

**Date**: 2026-04-19  
**Version**: 1.0  
**Status**: Planning Phase - Defines all classes before implementation  

---

## Part 1: Class Hierarchy Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SMPP SERVER CLASSES                          │
└─────────────────────────────────────────────────────────────────────┘

PRESENTATION LAYER
├── class SmppServer
├── class TcpServer
└── class SmppConnection

PROTOCOL LAYER
├── class SmppMessage (value object)
├── class SmppMessageParser (service)
├── class SmppMessageEncoder (service)
└── class SmppPduFramer (utility - may remove)

BUSINESS LOGIC LAYER
├── class SmppSession (state machine)
├── class SmppMessageProcessor (router)
├── class SmppRequestHandler (interface)
├── class BindReceiverHandler
├── class BindTransmitterHandler
├── class BindTransceiverHandler
├── class UnbindHandler
└── class EnquireLinkHandler

INFRASTRUCTURE LAYER
├── class IpValidator
├── class CredentialValidator
├── class DBusAuthenticator
├── class Logger
└── class SessionManager
```

---

## Part 2: Detailed Class Specifications

### PRESENTATION LAYER

---

#### Class: SmppServer

**Purpose**: Main application class. Owns the service lifecycle.

**Location**: `SMPPServer/include/smpp_server.hpp` (NEW)

**Responsibilities**:
- Initialize all components
- Start/stop TCP server
- Accept connections
- Graceful shutdown

**Members**:
```cpp
class SmppServer {
public:
    SmppServer(
        uint16_t port,
        const std::string& ip_config_path,
        const std::string& log_level);
    
    void start();
    void stop();
    bool is_running() const;
    
private:
    std::unique_ptr<TcpServer> tcp_server_;
    std::shared_ptr<IpValidator> ip_validator_;
    std::shared_ptr<CredentialValidator> cred_validator_;
    std::shared_ptr<DBusAuthenticator> dbus_auth_;
    std::shared_ptr<Logger> logger_;
    std::unique_ptr<SessionManager> session_mgr_;
    
    uint16_t port_;
    std::atomic<bool> running_;
};
```

**Dependencies**: TcpServer, IpValidator, CredentialValidator, DBusAuthenticator, Logger, SessionManager

---

#### Class: TcpServer

**Purpose**: Async TCP socket server using ASIO. Manages connections.

**Location**: `SMPPServer/include/tcp_server.hpp` (EXISTS - refactor)

**Responsibilities**:
- Listen on TCP port
- Accept incoming connections
- Create SmppConnection for each client
- Clean up closed connections

**Members**:
```cpp
class TcpServer {
public:
    TcpServer(
        asio::io_context& io_ctx,
        uint16_t port,
        std::shared_ptr<IpValidator> validator,
        std::function<void(std::shared_ptr<TcpSocket>)> on_connect);
    
    uint16_t port() const;
    void start();
    void stop();
    
private:
    void do_accept();
    void handle_connection(std::shared_ptr<asio::ip::tcp::socket> socket);
    
    asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<IpValidator> ip_validator_;
    std::function<void(std::shared_ptr<TcpSocket>)> on_connect_callback_;
};
```

**Dependencies**: IpValidator

---

#### Class: SmppConnection

**Purpose**: Manages one client connection. Routes messages to handlers.

**Location**: `SMPPServer/include/smpp_connection.hpp` (NEW)

**Responsibilities**:
- Read data from socket
- Parse incoming messages
- Route to appropriate handler
- Send responses
- Manage session lifecycle

**Members**:
```cpp
class SmppConnection : public std::enable_shared_from_this<SmppConnection> {
public:
    SmppConnection(
        std::shared_ptr<asio::ip::tcp::socket> socket,
        const std::string& client_ip,
        std::shared_ptr<SmppMessageProcessor> processor,
        std::shared_ptr<Logger> logger);
    
    void start();
    void stop();
    std::string session_id() const;
    std::string client_ip() const;
    SmppSession& session();
    
private:
    void on_socket_data(const asio::error_code& ec, std::size_t bytes);
    void process_message(const SmppMessage& msg);
    void send_response(const SmppMessage& resp);
    void handle_error(const std::string& error_msg);
    
    std::shared_ptr<asio::ip::tcp::socket> socket_;
    std::string client_ip_;
    std::string session_id_;
    std::shared_ptr<SmppSession> session_;
    std::unique_ptr<SmppMessageParser> parser_;
    std::shared_ptr<SmppMessageProcessor> processor_;
    std::shared_ptr<Logger> logger_;
    std::vector<uint8_t> read_buffer_;
};
```

**Dependencies**: SmppSession, SmppMessageParser, SmppMessageProcessor, Logger

---

### PROTOCOL LAYER

---

#### Class: SmppMessage (Value Object)

**Purpose**: Immutable representation of an SMPP message.

**Location**: `SMPPServer/include/smpp_message.hpp` (NEW)

**Responsibilities**:
- Store message data
- Provide type-safe accessors
- Serialize/deserialize

**Members**:
```cpp
class SmppMessage {
public:
    // Construction
    SmppMessage(
        uint32_t command_id,
        uint32_t command_status,
        uint32_t sequence_number,
        std::vector<uint8_t> body = {});
    
    // Accessors (const)
    uint32_t command_id() const { return command_id_; }
    uint32_t command_status() const { return command_status_; }
    uint32_t sequence_number() const { return sequence_number_; }
    const std::vector<uint8_t>& body() const { return body_; }
    
    // Type-safe queries
    bool is_bind_transmitter() const;
    bool is_bind_receiver() const;
    bool is_bind_transceiver() const;
    bool is_bind_response() const;
    bool is_unbind() const;
    bool is_enquire_link() const;
    
    // Field extraction (for BIND)
    std::string get_system_id() const;
    std::string get_password() const;
    
    // Serialization
    std::vector<uint8_t> encode() const;
    static SmppMessage decode(const std::vector<uint8_t>& bytes);
    
private:
    uint32_t command_id_;
    uint32_t command_status_;
    uint32_t sequence_number_;
    std::vector<uint8_t> body_;
};
```

**Dependencies**: None (pure data)

---

#### Class: SmppMessageParser (Service)

**Purpose**: Parse raw bytes into SmppMessage objects.

**Location**: `SMPPServer/include/smpp_message_parser.hpp` (NEW)

**Responsibilities**:
- Read bytes from buffer
- Extract header and body
- Validate structure
- Create SmppMessage objects

**Members**:
```cpp
class SmppMessageParser {
public:
    SmppMessageParser();
    
    // Feed bytes, get complete messages
    std::vector<SmppMessage> parse_bytes(
        const std::vector<uint8_t>& data);
    
    // Query state
    bool has_complete_pdu() const;
    size_t bytes_needed() const;
    
private:
    enum State {
        WAITING_HEADER,    // Need 16 bytes
        WAITING_BODY,      // Need body_len bytes
        PDU_COMPLETE       // Ready to extract
    };
    
    void parse_header();
    
    State state_;
    std::vector<uint8_t> buffer_;
    uint32_t command_length_;
    
    static uint32_t ntoh32(const uint8_t* ptr);
};
```

**Dependencies**: SmppMessage

---

#### Class: SmppMessageEncoder (Service)

**Purpose**: Convert SmppMessage to bytes for transmission.

**Location**: `SMPPServer/include/smpp_message_encoder.hpp` (NEW)

**Responsibilities**:
- Build SMPP response messages
- Encode to bytes
- Add proper headers

**Members**:
```cpp
class SmppMessageEncoder {
public:
    // Builder pattern for response messages
    static SmppMessage build_bind_transmitter_resp(
        uint32_t status,
        uint32_t sequence_number,
        const std::string& system_id = "TestServer");
    
    static SmppMessage build_bind_receiver_resp(
        uint32_t status,
        uint32_t sequence_number,
        const std::string& system_id = "TestServer");
    
    static SmppMessage build_bind_transceiver_resp(
        uint32_t status,
        uint32_t sequence_number,
        const std::string& system_id = "TestServer");
    
    static SmppMessage build_unbind_resp(
        uint32_t status,
        uint32_t sequence_number);
    
    static SmppMessage build_enquire_link_resp(
        uint32_t status,
        uint32_t sequence_number);
    
private:
    static SmppMessage build_response(
        uint32_t response_command_id,
        uint32_t status,
        uint32_t sequence_number,
        const std::vector<uint8_t>& body = {});
};
```

**Dependencies**: SmppMessage

---

### BUSINESS LOGIC LAYER

---

#### Class: SmppSession (State Machine)

**Purpose**: Model the state of one SMPP connection.

**Location**: `SMPPServer/include/smpp_session.hpp` (EXISTS - refactor)

**Responsibilities**:
- Track connection state
- Validate state transitions
- Store authenticated user
- Track sequence numbers

**Members**:
```cpp
class SmppSession {
public:
    enum class State {
        UNBOUND,              // Just connected, not bound
        BINDING,              // BIND in progress (optional intermediate state)
        BOUND_TRANSMITTER,    // Can send messages (client → server)
        BOUND_RECEIVER,       // Can receive messages (server → client)
        BOUND_TRANSCEIVER,    // Both directions
        UNBINDING,            // UNBIND in progress
        CLOSED                // Connection closed
    };
    
    SmppSession(const std::string& session_id, const std::string& client_ip);
    
    // Read-only queries
    std::string session_id() const;
    std::string client_ip() const;
    State state() const;
    std::string authenticated_as() const;
    bool is_bound() const;
    bool can_transmit() const;
    bool can_receive() const;
    std::time_t connected_at() const;
    std::time_t last_activity() const;
    
    // State transitions (return false if invalid)
    bool try_bind_as_transmitter(const std::string& username);
    bool try_bind_as_receiver(const std::string& username);
    bool try_bind_as_transceiver(const std::string& username);
    bool try_unbind();
    bool try_close();
    
    // Activity tracking
    void touch();  // Update last_activity timestamp
    
private:
    std::string session_id_;
    std::string client_ip_;
    State state_;
    std::string authenticated_as_;
    std::time_t connected_at_;
    std::time_t last_activity_;
};
```

**Dependencies**: None (pure domain logic)

---

#### Class: SmppMessageProcessor (Router)

**Purpose**: Route incoming messages to appropriate handlers.

**Location**: `SMPPServer/include/smpp_message_processor.hpp` (NEW)

**Responsibilities**:
- Select handler for message type
- Validate session state
- Call handler
- Return response or error

**Members**:
```cpp
class SmppMessageProcessor {
public:
    SmppMessageProcessor(
        std::shared_ptr<CredentialValidator> cred_validator,
        std::shared_ptr<DBusAuthenticator> dbus_auth,
        std::shared_ptr<Logger> logger);
    
    // Main processing function
    SmppMessage process_message(
        const SmppMessage& request,
        SmppSession& session);
    
private:
    SmppMessage handle_bind_transmitter(
        const SmppMessage& request,
        SmppSession& session);
    
    SmppMessage handle_bind_receiver(
        const SmppMessage& request,
        SmppSession& session);
    
    SmppMessage handle_bind_transceiver(
        const SmppMessage& request,
        SmppSession& session);
    
    SmppMessage handle_unbind(
        const SmppMessage& request,
        SmppSession& session);
    
    SmppMessage handle_enquire_link(
        const SmppMessage& request,
        SmppSession& session);
    
    SmppMessage handle_error(
        uint32_t command_id,
        uint32_t status,
        uint32_t sequence_number);
    
    std::shared_ptr<CredentialValidator> cred_validator_;
    std::shared_ptr<DBusAuthenticator> dbus_auth_;
    std::shared_ptr<Logger> logger_;
};
```

**Dependencies**: CredentialValidator, DBusAuthenticator, Logger, SmppSession, SmppMessage

---

#### Class: SmppRequestHandler (Interface)

**Purpose**: Base class for all command handlers (Strategy pattern).

**Location**: `SMPPServer/include/smpp_request_handler.hpp` (NEW)

**Responsibilities**:
- Define interface for handlers
- Handle one command type

**Members**:
```cpp
class SmppRequestHandler {
public:
    virtual ~SmppRequestHandler() = default;
    
    // Process request, return response
    virtual SmppMessage handle(
        const SmppMessage& request,
        SmppSession& session) = 0;
};
```

---

#### Class: BindTransmitterHandler

**Purpose**: Handle BIND_TRANSMITTER requests.

**Location**: `SMPPServer/include/handlers/bind_transmitter_handler.hpp` (NEW)

**Members**:
```cpp
class BindTransmitterHandler : public SmppRequestHandler {
public:
    BindTransmitterHandler(
        std::shared_ptr<DBusAuthenticator> auth,
        std::shared_ptr<Logger> logger);
    
    SmppMessage handle(
        const SmppMessage& request,
        SmppSession& session) override;
    
private:
    std::shared_ptr<DBusAuthenticator> auth_;
    std::shared_ptr<Logger> logger_;
};
```

---

#### Class: BindReceiverHandler, BindTransceiverHandler

**Purpose**: Handle BIND_RECEIVER and BIND_TRANSCEIVER.

**Similar structure to BindTransmitterHandler**

---

#### Class: UnbindHandler

**Purpose**: Handle UNBIND requests.

**Location**: `SMPPServer/include/handlers/unbind_handler.hpp` (NEW)

**Members**:
```cpp
class UnbindHandler : public SmppRequestHandler {
public:
    UnbindHandler(std::shared_ptr<Logger> logger);
    
    SmppMessage handle(
        const SmppMessage& request,
        SmppSession& session) override;
    
private:
    std::shared_ptr<Logger> logger_;
};
```

---

#### Class: EnquireLinkHandler

**Purpose**: Handle ENQUIRE_LINK (keep-alive) requests.

**Location**: `SMPPServer/include/handlers/enquire_link_handler.hpp` (NEW)

**Members**:
```cpp
class EnquireLinkHandler : public SmppRequestHandler {
public:
    EnquireLinkHandler(std::shared_ptr<Logger> logger);
    
    SmppMessage handle(
        const SmppMessage& request,
        SmppSession& session) override;
    
private:
    std::shared_ptr<Logger> logger_;
};
```

---

### INFRASTRUCTURE LAYER

---

#### Class: IpValidator

**Purpose**: Check if client IP is in whitelist.

**Location**: `SMPPServer/include/ip_validator.hpp` (EXISTS - refactor if needed)

**Responsibilities**:
- Load whitelist from config
- Check exact IPs
- Check CIDR ranges
- Reload on config change

**Members**:
```cpp
class IpValidator {
public:
    IpValidator(const std::string& config_path);
    
    bool is_allowed(const std::string& ip) const;
    void reload();
    
private:
    std::vector<std::string> exact_ips_;
    std::vector<std::pair<std::string, int>> cidr_ranges_;
    std::string config_path_;
    mutable std::mutex lock_;
};
```

---

#### Class: CredentialValidator

**Purpose**: Validate username/password.

**Location**: `SMPPServer/include/credential_validator.hpp` (NEW)

**Responsibilities**:
- Load credentials from config
- Hash and compare passwords
- Support multiple auth backends

**Members**:
```cpp
class CredentialValidator {
public:
    CredentialValidator(const std::string& config_path);
    
    bool validate(
        const std::string& username,
        const std::string& password) const;
    
private:
    std::map<std::string, std::string> credentials_; // username → hashed_password
    std::string config_path_;
};
```

---

#### Class: DBusAuthenticator

**Purpose**: Call D-Bus service for authentication.

**Location**: `SMPPServer/include/dbus_authenticator.hpp` (NEW)

**Responsibilities**:
- Connect to D-Bus system
- Call SMPPAuthenticator service
- Handle D-Bus errors

**Members**:
```cpp
class DBusAuthenticator {
public:
    DBusAuthenticator();
    ~DBusAuthenticator();
    
    bool authenticate(
        const std::string& username,
        const std::string& password) const;
    
private:
    // D-Bus connection (via sdbus-c++)
    std::unique_ptr<sdbus::IConnection> connection_;
    std::unique_ptr<sdbus::IProxy> authenticator_proxy_;
};
```

---

#### Class: Logger

**Purpose**: Centralized logging.

**Location**: `SMPPServer/include/logger.hpp` (EXISTS - may refactor)

**Responsibilities**:
- Log to file and console
- Support log levels
- Format messages

**Members**: Already implemented with spdlog wrapper

---

#### Class: SessionManager

**Purpose**: Manage all active sessions.

**Location**: `SMPPServer/include/session_manager.hpp` (NEW)

**Responsibilities**:
- Track all connected clients
- Create/remove sessions
- Query session info

**Members**:
```cpp
class SessionManager {
public:
    std::shared_ptr<SmppSession> create_session(
        const std::string& session_id,
        const std::string& client_ip);
    
    void remove_session(const std::string& session_id);
    
    std::shared_ptr<SmppSession> get_session(
        const std::string& session_id) const;
    
    std::vector<std::string> get_all_session_ids() const;
    
    size_t active_connection_count() const;
    
private:
    std::map<std::string, std::shared_ptr<SmppSession>> sessions_;
    mutable std::mutex lock_;
};
```

---

## Part 3: Class Dependency Graph

```
SmppServer
    ├─ TcpServer
    │   └─ IpValidator
    ├─ SmppConnection (created per client)
    │   ├─ SmppSession
    │   ├─ SmppMessageParser
    │   └─ SmppMessageProcessor
    │       ├─ CredentialValidator
    │       ├─ DBusAuthenticator
    │       ├─ BindTransmitterHandler
    │       ├─ BindReceiverHandler
    │       ├─ BindTransceiverHandler
    │       ├─ UnbindHandler
    │       └─ EnquireLinkHandler
    ├─ IpValidator
    ├─ CredentialValidator
    ├─ DBusAuthenticator
    ├─ Logger
    └─ SessionManager
```

---

## Part 4: File Organization

```
SMPPServer/
├── include/
│   ├── smpp_server.hpp              [NEW] - Main application
│   ├── smpp_connection.hpp          [NEW] - Per-client handler
│   ├── tcp_server.hpp               [EXISTS] - Socket server
│   ├── smpp_session.hpp             [EXISTS] - Refactor
│   ├── ip_validator.hpp             [EXISTS]
│   ├── credential_validator.hpp     [NEW] - Password validation
│   ├── dbus_authenticator.hpp       [NEW] - D-Bus calls
│   ├── logger.hpp                   [EXISTS]
│   ├── session_manager.hpp          [NEW] - Session tracking
│   ├── smpp_message.hpp             [NEW] - Value object
│   ├── smpp_message_parser.hpp      [NEW] - PDU parsing
│   ├── smpp_message_encoder.hpp     [NEW] - Response building
│   ├── smpp_message_processor.hpp   [NEW] - Message routing
│   ├── smpp_request_handler.hpp     [NEW] - Handler base
│   └── handlers/
│       ├── bind_transmitter_handler.hpp [NEW]
│       ├── bind_receiver_handler.hpp    [NEW]
│       ├── bind_transceiver_handler.hpp [NEW]
│       ├── unbind_handler.hpp           [NEW]
│       └── enquire_link_handler.hpp     [NEW]
│
└── src/
    ├── main.cpp                     [EXISTS]
    ├── smpp_server.cpp              [NEW]
    ├── smpp_connection.cpp          [NEW]
    ├── tcp_server.cpp               [EXISTS] - Refactor
    ├── smpp_session.cpp             [EXISTS] - Refactor
    ├── credential_validator.cpp     [NEW]
    ├── dbus_authenticator.cpp       [NEW]
    ├── session_manager.cpp          [NEW]
    ├── smpp_message.cpp             [NEW]
    ├── smpp_message_parser.cpp      [NEW]
    ├── smpp_message_encoder.cpp     [NEW]
    ├── smpp_message_processor.cpp   [NEW]
    └── handlers/
        ├── bind_transmitter_handler.cpp [NEW]
        ├── bind_receiver_handler.cpp    [NEW]
        ├── bind_transceiver_handler.cpp [NEW]
        ├── unbind_handler.cpp           [NEW]
        └── enquire_link_handler.cpp     [NEW]
```

---

## Part 5: Summary - Classes by Category

| Category | Class | Status |
|---|---|---|
| **Entry Point** | SmppServer | NEW |
| **Connection** | TcpServer | EXISTS (refactor) |
| **Per-Client** | SmppConnection | NEW |
| **Message** | SmppMessage | NEW |
| **Parser** | SmppMessageParser | NEW |
| **Encoder** | SmppMessageEncoder | NEW |
| **Session** | SmppSession | EXISTS (refactor) |
| **Router** | SmppMessageProcessor | NEW |
| **Handlers** | 5× Handler classes | NEW |
| **Validation** | IpValidator, CredentialValidator | EXISTS/NEW |
| **D-Bus** | DBusAuthenticator | NEW |
| **Logging** | Logger | EXISTS |
| **Management** | SessionManager | NEW |

**Total**: 18 classes (8 existing + refactor, 10 completely new)

---

**Document Status**: Complete - Ready for Sequence Diagrams  
**Next Step**: Create sequence diagrams for main flows  
**Owner**: Architecture Team  
