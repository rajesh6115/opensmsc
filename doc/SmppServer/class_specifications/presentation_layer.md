# Presentation Layer: Class Specifications

**Date**: 2026-04-19  
**Version**: 1.0  
**Layer**: Presentation Layer (Network I/O)  
**Classes**: TcpServer, SmppConnection, SmppServer  

---

## Overview

The Presentation Layer handles network I/O and manages SMPP client connections using ASIO async I/O.

- **TcpServer**: ASIO acceptor, listens on port 2775, accepts connections
- **SmppConnection**: Per-client async state machine, reads/writes SMPP PDUs
- **SmppServer**: Orchestrator, creates and manages components

---

## Class 1: TcpServer

### Purpose
Accept TCP connections on port 2775 (IPv6 with dual-stack), validate IP whitelist, create per-client SmppConnection instances.

### Location
- **Header**: `SMPPServer/include/tcp_server.hpp`
- **Source**: `SMPPServer/src/tcp_server.cpp`

### Responsibilities

**Server Startup**:
- Bind to [::]:2775 (IPv6 dual-stack, listening on both IPv4 and IPv6)
- Start async_accept loop
- Log listening status

**Connection Acceptance**:
- Accept incoming TCP connection
- Extract client IP address
- Validate IP against whitelist (IpValidator)
- If valid: create SmppConnection, continue accepting
- If invalid: close socket, log rejection, continue accepting

**Connection Cleanup**:
- Track active connections
- On client disconnect: remove from tracking, clean up resources
- Graceful shutdown: close all active connections, stop acceptor

### Class Signature

```cpp
class TcpServer {
public:
    TcpServer(
        asio::io_context& io_context,
        uint16_t port,
        std::shared_ptr<IpValidator> ip_validator,
        std::shared_ptr<SmppMessageProcessor> message_processor,
        std::shared_ptr<SessionManager> session_manager,
        std::shared_ptr<Logger> logger);
    
    // Lifecycle
    void start();  // Begin async_accept loop
    void stop();   // Stop accepting, close all connections
    
    // Queries
    size_t active_connection_count() const;
    
private:
    // ASIO types
    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    
    // Async callback chain
    void on_accept(
        const asio::error_code& ec,
        asio::ip::tcp::socket peer_socket);
    
    void restart_accept();  // Restart async_accept after handling one
    
    // Dependencies
    std::shared_ptr<IpValidator> ip_validator_;
    std::shared_ptr<SmppMessageProcessor> message_processor_;
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<Logger> logger_;
    
    // Active connections
    std::set<std::shared_ptr<SmppConnection>> active_connections_;
    std::mutex connections_mutex_;
};
```

### Processing Flow

```
TcpServer::start()
  └─ async_accept()

on_accept(ec, socket)
  ├─ if (ec) { log error, restart_accept() }
  │
  ├─ Extract peer_ip from socket.remote_endpoint()
  │
  ├─ IpValidator::is_whitelisted(peer_ip)
  │  ├─ if (false) { Log WARN "IP rejected", close socket, goto restart }
  │  └─ if (true) { continue }
  │
  ├─ Create session_id (uuid or counter)
  │
  ├─ Create SmppSession(session_id, peer_ip)
  │
  ├─ Create SmppConnection(
  │       socket,
  │       session,
  │       message_processor,
  │       session_manager,
  │       logger)
  │
  ├─ SmppConnection::start() [async I/O chain begins]
  │
  ├─ Track in active_connections_
  │
  └─ restart_accept() [accept next client]
```

### Key Implementation Details

**ASIO Acceptor Setup**:
```cpp
TcpServer::TcpServer(...)
    : io_context_(io_context),
      acceptor_(io_context,
                asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port)) {
    
    // Enable dual-stack (IPv4 on IPv6 socket)
    acceptor_.set_option(asio::ip::v6_only(false));
}
```

**Connection Acceptance Loop**:
```cpp
void TcpServer::start() {
    restart_accept();
}

void TcpServer::restart_accept() {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    acceptor_.async_accept(
        *socket,
        [this, socket](const asio::error_code& ec) {
            on_accept(ec, std::move(*socket));
        });
}

void TcpServer::on_accept(
    const asio::error_code& ec,
    asio::ip::tcp::socket peer_socket) {
    
    if (ec) {
        logger_->error("Accept error: {}", ec.message());
        restart_accept();
        return;
    }
    
    std::string peer_ip = peer_socket.remote_endpoint()
        .address().to_string();
    
    if (!ip_validator_->is_whitelisted(peer_ip)) {
        logger_->warn("IP rejected: {}", peer_ip);
        restart_accept();
        return;
    }
    
    auto session = std::make_shared<SmppSession>(
        "sess_" + std::to_string(++session_counter_),
        peer_ip);
    
    auto connection = std::make_shared<SmppConnection>(
        std::move(peer_socket),
        session,
        message_processor_,
        session_manager_,
        logger_);
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        active_connections_.insert(connection);
    }
    
    connection->start();
    restart_accept();
}
```

### Dependencies
- ASIO
- IpValidator
- SmppMessageProcessor
- SessionManager
- Logger

---

## Class 2: SmppConnection

### Purpose
Per-client async state machine. Reads SMPP PDU header/body, assembles complete PDU, routes to message processor, writes responses.

### Location
- **Header**: `SMPPServer/include/smpp_connection.hpp`
- **Source**: `SMPPServer/src/smpp_connection.cpp`

### Responsibilities

**Async I/O Management**:
- Read PDU header (16 bytes)
- Read PDU body (variable length)
- Assemble complete PDU
- Route to message processor
- Write response PDU

**Connection Lifecycle**:
- Start async read chain
- Handle disconnects (EOF, errors)
- Clean up resources (remove from TcpServer tracking)

**Activity Tracking**:
- Touch SmppSession on every message (for idle detection)
- Handle keep-alives (ENQUIRE_LINK)

### Class Signature

```cpp
class SmppConnection : public std::enable_shared_from_this<SmppConnection> {
public:
    SmppConnection(
        asio::ip::tcp::socket socket,
        std::shared_ptr<SmppSession> session,
        std::shared_ptr<SmppProtocolAdapter> protocol_adapter,
        std::shared_ptr<SmppMessageProcessor> message_processor,
        std::shared_ptr<SessionManager> session_manager,
        std::shared_ptr<Logger> logger);
    
    // Lifecycle
    void start();  // Begin async read chain
    void stop();   // Stop reading, close socket
    
    // Queries
    const std::string& session_id() const;
    const std::string& client_ip() const;
    bool is_connected() const;
    
private:
    // ASIO state
    asio::ip::tcp::socket socket_;
    std::vector<uint8_t> header_buffer_;  // 16 bytes
    std::vector<uint8_t> body_buffer_;    // variable
    
    // Session & processing
    std::shared_ptr<SmppSession> session_;
    std::shared_ptr<SmppProtocolAdapter> protocol_adapter_;  // Handles TCP → smppcxx conversion
    std::shared_ptr<SmppMessageProcessor> message_processor_;
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<Logger> logger_;
    
    // Async read chain
    void read_pdu_header();
    void on_header_read(const asio::error_code& ec, size_t bytes_read);
    void read_pdu_body(uint32_t body_length);
    void on_body_read(const asio::error_code& ec, size_t bytes_read);
    
    // Response write
    void write_pdu(const std::vector<uint8_t>& pdu_bytes);
    void on_write(const asio::error_code& ec, size_t bytes_written);
    
    // Cleanup
    void on_disconnect(const std::string& reason);
};
```

### Processing Flow

```
SmppConnection::start()
  └─ read_pdu_header()

read_pdu_header()
  └─ async_read(socket_, header_buffer_)
     └─ on_header_read(ec, bytes_read)
  
on_header_read()
  ├─ if (bytes_read != 16) { on_disconnect("incomplete header"), return }
  ├─ Extract command_length from header bytes [0:4]
  ├─ body_length = command_length - 16
  ├─ if (body_length < 0) { on_disconnect("invalid length"), return }
  │
  ├─ if (body_length == 0)
  │  └─ Dispatch zero-length body (e.g., UNBIND with no params)
  │
  └─ read_pdu_body(body_length)

read_pdu_body(body_length)
  └─ async_read(socket_, body_buffer_, transfer_exactly(body_length))
     └─ on_body_read(ec, bytes_read)

on_body_read(ec, bytes_read)
  ├─ if (bytes_read != body_length) { on_disconnect("incomplete body"), return }
  │
  ├─ Combine header_buffer_ + body_buffer_ into full_pdu
  │
  ├─ session_->touch() [update last_activity]
  │
  ├─ std::vector<Smpp::Header*> pdu_objects = 
  │   protocol_adapter_->process_tcp_data(full_pdu)
  │   [Converts bytes to smppcxx objects: BindReceiver, BindTransmitter, etc.]
  │
  ├─ For each pdu in pdu_objects:
  │   ├─ Smpp::Header* response = message_processor_->process_pdu(
  │   │       std::move(pdu), *session_)
  │   │   [Routes to correct handler, returns smppcxx response]
  │   │
  │   └─ std::vector<uint8_t> response_bytes = 
  │       protocol_adapter_->encode_response(*response)
  │       [Encodes smppcxx response to bytes]
  │
  ├─ write_pdu(response_bytes)
  │
  ├─ Clear buffers
  │
  └─ read_pdu_header() [continue chain for next PDU]

write_pdu(response_bytes)
  └─ async_write(socket_, response_bytes)
     └─ on_write(ec, bytes_written)

on_write(ec, bytes_written)
  ├─ if (ec) { on_disconnect("write error"), return }
  │
  └─ [continue next read, handled in on_body_read()]

on_disconnect(reason)
  ├─ Logger::info("Client {} disconnected: {}", session_id_, reason)
  ├─ session_manager_->remove_session(session_id_)
  ├─ socket_.close()
  └─ [TcpServer removes from active_connections_ set]
```

### Key Implementation Details

**Header/Body Parsing**:
```cpp
void SmppConnection::on_header_read(
    const asio::error_code& ec,
    size_t bytes_read) {
    
    if (ec || bytes_read != 16) {
        on_disconnect("incomplete header");
        return;
    }
    
    // Extract command_length (big-endian, bytes 0-3)
    uint32_t command_length = 
        (static_cast<uint32_t>(header_buffer_[0]) << 24) |
        (static_cast<uint32_t>(header_buffer_[1]) << 16) |
        (static_cast<uint32_t>(header_buffer_[2]) << 8) |
        (static_cast<uint32_t>(header_buffer_[3]));
    
    uint32_t body_length = command_length - 16;
    if (body_length > 65535) {  // Sanity check
        on_disconnect("PDU too large");
        return;
    }
    
    read_pdu_body(body_length);
}

void SmppConnection::on_body_read(
    const asio::error_code& ec,
    size_t bytes_read) {
    
    if (ec || bytes_read != body_buffer_.size()) {
        on_disconnect("incomplete body");
        return;
    }
    
    // Assemble full PDU
    std::vector<uint8_t> full_pdu = header_buffer_;
    full_pdu.insert(full_pdu.end(), body_buffer_.begin(), body_buffer_.end());
    
    // Track activity
    session_->touch();
    
    // Process message
    try {
        SmppMessage request = SmppMessage::decode(full_pdu);
        SmppMessage response = message_processor_->process_message(request, *session_);
        std::vector<uint8_t> response_bytes = response.encode();
        
        write_pdu(response_bytes);
    } catch (const std::exception& ex) {
        logger_->error("Message processing error: {}", ex.what());
        on_disconnect("processing error");
    }
}
```

**Enable Shared From This** (for keeping connection alive while async operation pending):
```cpp
class SmppConnection : public std::enable_shared_from_this<SmppConnection> {
    // ...
    void read_pdu_header() {
        asio::async_read(
            socket_,
            asio::buffer(header_buffer_),
            [self = shared_from_this()](const asio::error_code& ec, size_t bytes) {
                self->on_header_read(ec, bytes);
            });
    }
};
```

### Dependencies
- ASIO
- smppcxx (via SmppProtocolAdapter)
- SmppProtocolAdapter
- SmppSession
- SmppMessageProcessor
- SessionManager
- Logger

---

## Class 3: SmppServer

### Purpose
Application entry point. Creates and manages all components. Handles graceful shutdown.

### Location
- **Header**: `SMPPServer/include/smpp_server.hpp`
- **Source**: `SMPPServer/src/smpp_server.cpp`

### Responsibilities

**Component Creation & DI**:
- Create ASIO io_context
- Create Logger (spdlog initialization)
- Create IpValidator, CredentialValidator, DBusAuthenticator
- Create SmppMessageProcessor with all handlers
- Create SessionManager
- Create TcpServer
- Inject all dependencies

**Server Lifecycle**:
- start() → begin accepting connections
- stop() → graceful shutdown
- run() → io_context.run() (blocking, handles all async events)

**Configuration**:
- Load IP whitelist
- Load log level
- Bind port (default 2775)

### Class Signature

```cpp
class SmppServer {
public:
    SmppServer();
    
    // Lifecycle
    void start(
        uint16_t port = 2775,
        const std::string& log_level = "info");
    
    void stop();
    
    void run();  // Block on io_context.run()
    
private:
    // ASIO
    asio::io_context io_context_;
    
    // Components (created in start())
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<IpValidator> ip_validator_;
    std::shared_ptr<CredentialValidator> cred_validator_;
    std::shared_ptr<DBusAuthenticator> dbus_auth_;
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<SmppMessageProcessor> message_processor_;
    std::shared_ptr<TcpServer> tcp_server_;
};
```

### Processing Flow

```
main()
  ├─ Parse command-line args
  │  ├─ --port=2775
  │  └─ --log-level=info
  │
  ├─ SmppServer server;
  │
  ├─ server.start(port, log_level)
  │  ├─ Logger::init(log_path, log_level)
  │  ├─ Create all validators & authenticators
  │  ├─ Create all handlers
  │  ├─ Create SmppMessageProcessor(validators, handlers, logger)
  │  ├─ Create TcpServer(io_context, port, validators, processor, session_manager, logger)
  │  └─ TcpServer::start()
  │
  ├─ server.run()
  │  └─ io_context.run() [blocks, handles all async events]
  │
  └─ On SIGTERM: server.stop()
     ├─ TcpServer::stop()
     └─ io_context.stop()
```

### Key Implementation Details

**Dependency Injection Container**:
```cpp
void SmppServer::start(
    uint16_t port,
    const std::string& log_level) {
    
    // Logging
    logger_ = std::make_shared<Logger>();
    logger_->init(DEFAULT_LOG_FILE, log_level);
    
    // Validators
    ip_validator_ = std::make_shared<IpValidator>();
    ip_validator_->load_whitelist(WHITELIST_FILE);
    
    cred_validator_ = std::make_shared<CredentialValidator>();
    
    // D-Bus authenticator
    dbus_auth_ = std::make_shared<DBusAuthenticator>(
        DBUS_AUTHENTICATOR_SERVICE);
    
    // Session tracking
    session_manager_ = std::make_shared<SessionManager>();
    
    // Message processor with all handlers
    auto bind_tx_handler = std::make_shared<BindTransmitterHandler>(
        dbus_auth_, logger_);
    auto bind_rx_handler = std::make_shared<BindReceiverHandler>(
        dbus_auth_, logger_);
    auto bind_trx_handler = std::make_shared<BindTransceiverHandler>(
        dbus_auth_, logger_);
    auto unbind_handler = std::make_shared<UnbindHandler>(logger_);
    auto enquire_link_handler = std::make_shared<EnquireLinkHandler>(logger_);
    
    message_processor_ = std::make_shared<SmppMessageProcessor>(
        cred_validator_, dbus_auth_, logger_);
    message_processor_->set_handlers(
        bind_tx_handler, bind_rx_handler, bind_trx_handler,
        unbind_handler, enquire_link_handler);
    
    // TCP server
    tcp_server_ = std::make_shared<TcpServer>(
        io_context_,
        port,
        ip_validator_,
        message_processor_,
        session_manager_,
        logger_);
    
    tcp_server_->start();
    
    logger_->info("SMPPServer started on port {}", port);
}

void SmppServer::run() {
    io_context_.run();  // Blocks until stop() called
}

void SmppServer::stop() {
    io_context_.stop();
    logger_->info("SMPPServer stopped");
    logger_->shutdown();
}
```

### Dependencies
- ASIO
- smppcxx
- Logger
- IpValidator
- CredentialValidator
- DBusAuthenticator
- SessionManager
- SmppProtocolAdapter
- SmppMessageProcessor
- TcpServer

---

## Dependencies Summary

**TcpServer**:
- ASIO
- IpValidator
- SmppMessageProcessor
- SessionManager
- Logger

**SmppConnection**:
- ASIO
- SmppMessage
- SmppSession
- SmppMessageProcessor
- SessionManager
- Logger

**SmppServer**:
- ASIO
- Logger
- IpValidator
- CredentialValidator
- DBusAuthenticator
- SessionManager
- SmppMessageProcessor
- TcpServer
- All handlers

---

## Testing Checklist

- [ ] TcpServer accepts valid IPv4 connections
- [ ] TcpServer accepts valid IPv6 connections
- [ ] TcpServer rejects non-whitelisted IPs
- [ ] TcpServer creates SmppConnection per client
- [ ] SmppConnection reads complete SMPP PDU (header + body)
- [ ] SmppConnection handles chunked input (partial header, then body)
- [ ] SmppConnection routes to message processor
- [ ] SmppConnection writes response
- [ ] SmppConnection handles client disconnect
- [ ] SmppServer starts and stops gracefully
- [ ] Multiple concurrent clients work correctly
- [ ] Activity tracking (session->touch()) works

---

**Document Status**: Complete  
**Next**: infrastructure_layer.md
