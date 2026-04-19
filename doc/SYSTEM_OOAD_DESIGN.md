# System-Wide OOAD Design Analysis: Simple SMPP Server Platform

**Date**: 2026-04-19  
**Version**: 1.0  
**Scope**: Complete platform architecture (not just SMPPServer binary)  
**Status**: CRITICAL - Foundation for all development  

---

## Executive Summary

This document provides a comprehensive Object-Oriented design for the entire SMPP platform, including:
- Multiple service components (SMPPServer, SMPPAuthenticator, others)
- Inter-service communication (D-Bus)
- Data flow across boundaries
- Deployment architecture (Docker, systemd)
- Testing strategy

---

## Part 1: System Context & Scope

### What Are We Building?

A **production SMPP platform** that provides:
1. **SMPP Gateway Service** - Accepts SMPP client connections
2. **Authentication Service** - Validates credentials via D-Bus
3. **Logging Service** - Centralized logging across all services
4. **Configuration Service** - IP whitelist, credentials, settings
5. **Testing Suite** - Unit, integration, load tests

### Stakeholders
- **SMS Service Provider**: Need reliable SMPP gateway
- **Operations Team**: Need monitoring, logs, easy deployment
- **Developers**: Need clean APIs, easy to extend
- **Security**: Need audit trail, IP validation, credential security

### Non-Functional Requirements
| Requirement | Target |
|---|---|
| Concurrent Connections | 1000+ |
| Message Throughput | 100+ msg/sec |
| Latency (BIND) | <500ms |
| Uptime | 99.9% |
| Mean Time to Recovery | <5min |
| Logging | All connections, all messages (debug), errors always |
| Audit Trail | Who connected, when, result |

---

## Part 2: System Architecture Overview

### High-Level Component Model

```
┌────────────────────────────────────────────────────────────┐
│                    HOST OS                                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ systemd (Process Management)                         │  │
│  │  └─ simple_smpp_server.service                       │  │
│  │  └─ smpp_authenticator.service                       │  │
│  │  └─ smpp_logger.service                              │  │
│  └──────────────────────────────────────────────────────┘  │
│                         ↕                                    │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ D-Bus (IPC Bus)                                      │  │
│  │  ├─ /com/telecom/SMPPServer                          │  │
│  │  ├─ /com/telecom/SMPPAuthenticator                   │  │
│  │  └─ /com/telecom/SMPPLogger                          │  │
│  └──────────────────────────────────────────────────────┘  │
│                         ↕                                    │
│  ┌──────────┐  ┌──────────────┐  ┌──────────┐              │
│  │ Config   │  │ Credential   │  │ Audit    │              │
│  │ Files    │  │ Store        │  │ Log      │              │
│  └──────────┘  └──────────────┘  └──────────┘              │
└────────────────────────────────────────────────────────────┘
         ↑                                        ↑
   [TCP Port 2775]                      [Log files, DB]
         ↓                                        ↓
    ┌─────────┐                            ┌──────────┐
    │ SMPP    │                            │Monitoring│
    │Clients  │                            │ System   │
    └─────────┘                            └──────────┘
```

### Deployment Architecture (Docker)

```
Docker Container: smsc-dev-container
├── /workspace (volume mount - source code)
├── /var/log/simple_smpp_server (volume mount - logs)
├── /etc/simple_smpp_server (volume mount - config)
└── Services
    ├── SMPPServer (C++)
    │   ├─ Listens: 0.0.0.0:2775 (SMPP protocol)
    │   ├─ D-Bus Client: Calls SMPPAuthenticator
    │   └─ Logs: /var/log/simple_smpp_server/server.log
    │
    ├── SMPPAuthenticator (C++)
    │   ├─ D-Bus Service: /com/telecom/SMPPAuthenticator
    │   ├─ Method: Authenticate(username, password) → bool
    │   └─ Reads: /etc/simple_smpp_server/credentials
    │
    ├── SMPPLogger (C++)
    │   ├─ D-Bus Service: /com/telecom/SMPPLogger
    │   ├─ Method: LogMessage(timestamp, level, component, msg)
    │   └─ Writes: /var/log/simple_smpp_server/all.log
    │
    └── dbus-daemon
        └─ Provides: org.freedesktop.DBus (system bus)
```

---

## Part 3: Core Domain Model (System-Wide)

### 3.1 Connection Lifecycle

```
Timeline:
    [Client connects]
         ↓
    TcpServer accepts connection
         ↓
    IP Validator checks whitelist
         ├─ REJECT → close socket
         └─ ALLOW → create SmppConnection
              ↓
         [Client sends BIND_TRANSMITTER]
              ↓
         SmppConnection.on_message(bind_pdu)
              ├─ Parse: extract username, password
              ├─ D-Bus call: SMPPAuthenticator.Authenticate(user, pass)
              │    └─ SMPPAuthenticator queries credential store
              │        └─ returns true/false
              ├─ if valid → SmppSession.set_bound_transmitter()
              └─ send BIND_TRANSMITTER_RESP
              ↓
         [Client sends ENQUIRE_LINK]
              ↓
         SmppConnection.on_message(enquire_link_pdu)
              └─ send ENQUIRE_LINK_RESP
              ↓
         [Client sends UNBIND]
              ↓
         SmppConnection.on_message(unbind_pdu)
              ├─ SmppSession.set_unbound()
              ├─ send UNBIND_RESP
              └─ close socket
              ↓
         [Connection closed]
```

### 3.2 Message Processing Pipeline

```
Raw TCP Data
    ↓
[SmppConnection Layer]
    ├─ TcpSocket.read() → bytes[]
    ├─ SmppMessageParser.parse_bytes() → SmppMessage[]
    └─ for each message → process_message()
         ↓
[SmppMessageProcessor Layer]
    ├─ Validate message is well-formed
    ├─ Check SmppSession state (are we bound?)
    ├─ Select appropriate handler (BindHandler, UnbindHandler, etc.)
    └─ Call handler.handle(message)
         ↓
[Handler Layer]
    ├─ BIND: Call D-Bus SMPPAuthenticator
    ├─ UNBIND: Update session state
    ├─ ENQUIRE_LINK: Send keep-alive response
    └─ SUBMIT_SM: (Phase 1.2) Queue message for transmission
         ↓
[Response Layer]
    ├─ SmppMessageBuilder.build_response() → SmppMessage
    ├─ SmppMessageEncoder.encode() → bytes[]
    └─ TcpSocket.write(bytes)
         ↓
[Logging Layer]
    └─ D-Bus call: SMPPLogger.LogMessage(event details)
```

### 3.3 Key Domain Objects

#### SmppMessage
```cpp
class SmppMessage {
    // Core fields from SMPP header
    uint32_t command_id;       // BIND_TRANSMITTER, UNBIND, etc.
    uint32_t command_status;   // 0 = success, >0 = error code
    uint32_t sequence_number;  // Unique per session
    std::vector<uint8_t> body; // Command-specific data
    
    // Type-safe queries
    CommandType type() const;
    bool is_request() const;
    bool is_response() const;
    
    // Field extraction
    std::string get_system_id() const;     // From BIND
    std::string get_password() const;      // From BIND
    std::string get_message_text() const;  // From SUBMIT_SM
    
    // Serialization
    std::vector<uint8_t> encode() const;
};
```

#### SmppSession
```cpp
class SmppSession {
    enum State { UNBOUND, BINDING, BOUND_TX, BOUND_RX, BOUND_TRX, UNBINDING };
    
    // Read-only after initialization
    std::string session_id;      // Unique UUID
    std::string client_ip;       // Remote IP
    time_t connected_at;         // Connection timestamp
    
    // Mutable state
    State state;                 // Connection state
    std::string authenticated_as; // Username after successful BIND
    std::vector<uint32_t> sequence_history; // For tracking requests
    
    // State transition methods (fail if invalid)
    bool try_bind_as_transmitter(const std::string& username);
    bool try_bind_as_receiver(const std::string& username);
    bool try_unbind();
    
    // Query methods
    bool is_bound() const;
    bool can_transmit() const;
    bool can_receive() const;
    time_t idle_seconds() const;
};
```

#### SmppConnection
```cpp
class SmppConnection {
    std::shared_ptr<TcpSocket> socket;
    std::shared_ptr<SmppSession> session;
    std::unique_ptr<SmppMessageParser> parser;
    std::unique_ptr<SmppMessageProcessor> processor;
    std::shared_ptr<DBusAuthenticator> dbus_auth;
    std::shared_ptr<Logger> logger;
    
    // Main event loop
    void on_socket_data_available(size_t bytes);
    
private:
    void process_message(const SmppMessage& msg);
    void send_response(const SmppMessage& response);
    SmppMessage handle_bind(const SmppMessage& bind_req);
    SmppMessage handle_unbind(const SmppMessage& unbind_req);
};
```

#### SmppServer
```cpp
class SmppServer {
    std::unique_ptr<TcpServer> tcp_server;
    std::shared_ptr<IpValidator> ip_validator;
    std::shared_ptr<DBusAuthenticator> dbus_auth;
    std::shared_ptr<Logger> logger;
    std::map<std::string, std::shared_ptr<SmppConnection>> connections;
    
    // Start listening
    void start(uint16_t port);
    void stop();
    
    // Connection lifecycle
    void on_client_connected(std::shared_ptr<TcpSocket> socket);
    void on_client_disconnected(const std::string& session_id);
};
```

---

## Part 4: Service-to-Service Interactions

### 4.1 SMPPServer ↔ SMPPAuthenticator (D-Bus)

```
SMPPServer (Client)
    ↓
[D-Bus IPC]
    object: /com/telecom/SMPPAuthenticator
    interface: com.telecom.SMPPAuthenticator
    method: Authenticate(s username, s password) → b success
    ↓
SMPPAuthenticator (Service)
    ├─ Load credentials from /etc/simple_smpp_server/credentials.conf
    ├─ Look up username
    ├─ Hash provided password with stored salt
    ├─ Compare hashes
    └─ Return result
```

**Sequence Diagram**:
```
Client          SMPPServer        SMPPAuthenticator
  │                 │                    │
  ├─BIND_TX────────→│                    │
  │                 │                    │
  │                 │─Authenticate()────→│
  │                 │  (user, pass)      │
  │                 │                    │ [Load creds]
  │                 │                    │ [Hash & compare]
  │                 │←──true/false────────│
  │                 │                    │
  │            [Update session]          │
  │                 │                    │
  │←──BIND_TX_RESP──│                    │
  │                 │                    │
```

### 4.2 SMPPServer → SMPPLogger (D-Bus)

```
SMPPServer (Client)
    ↓
[D-Bus IPC]
    object: /com/telecom/SMPPLogger
    interface: com.telecom.SMPPLogger
    method: LogMessage(
        s timestamp,    // "2026-04-19 17:30:15.123"
        s level,        // "INFO", "WARN", "ERROR"
        s component,    // "SmppHandler", "TcpServer"
        s message       // "received BIND_TRANSMITTER from 127.0.0.1"
    )
    ↓
SMPPLogger (Service)
    ├─ Format message with timestamp
    ├─ Write to /var/log/simple_smpp_server/all.log
    └─ Send to syslog (optional)
```

---

## Part 5: Data Persistence & Configuration

### 5.1 Configuration Files

```
/etc/simple_smpp_server/
├── allowed_ips.conf
│   ├─ Format: one IP/CIDR per line
│   ├─ Example: 127.0.0.1, ::1, 192.168.1.0/24
│   └─ Reload: HUP signal
│
├── credentials.conf
│   ├─ Format: username:password_hash:salt
│   ├─ Example: test:$2b$12$...hash...:salt123
│   └─ No reload (requires restart)
│
└── server.conf
    ├─ port=2775
    ├─ log_level=info
    ├─ max_connections=10000
    └─ bind_timeout=5000ms
```

### 5.2 Log Files

```
/var/log/simple_smpp_server/
├── server.log
│   ├─ Rotating (10MB × 5 files)
│   ├─ Format: [ISO8601] [LEVEL] [Component] message
│   └─ Contains: Bind/unbind, keep-alives, errors
│
├── audit.log
│   ├─ Non-rotating (append-only)
│   ├─ Format: CSV with timestamp, action, result
│   └─ Contains: All connection attempts, auth success/fail
│
└── debug.log
    ├─ Rotating (when log_level=debug)
    ├─ Format: Hex dumps, PDU details, state transitions
    └─ Contains: Everything (verbose)
```

---

## Part 6: Component Responsibilities (SOLID)

| Component | Responsibility | Dependencies |
|---|---|---|
| **SmppServer** | Start/stop service, accept connections | TcpServer, IpValidator, Logger |
| **TcpServer** | Manage sockets, read/write | ASIO, SmppConnection |
| **SmppConnection** | Handle one client connection | SmppSession, Processor, Parser |
| **SmppSession** | Model connection state | (none - value object) |
| **SmppMessageParser** | Parse SMPP protocol | (none - pure function) |
| **SmppMessageProcessor** | Route messages to handlers | Handlers, Session |
| **BindHandler** | Handle BIND requests | DBusAuthenticator, Session |
| **UnbindHandler** | Handle UNBIND requests | Session |
| **EnquireLinkHandler** | Handle ENQUIRE_LINK requests | (none) |
| **IpValidator** | Check whitelist | Config file |
| **DBusAuthenticator** | Call D-Bus service | D-Bus system |
| **Logger** | Output logs | spdlog |

---

## Part 7: Testing Strategy

### 7.1 Test Pyramid

```
                    ╱╲
                   ╱  ╲        E2E Tests (5%)
                  ╱────╲       - Full platform in Docker
                 ╱      ╲      - External SMPP clients
                ╱════════╲
               ╱          ╲    Integration Tests (20%)
              ╱  Handlers  ╲   - D-Bus calls
             ╱  Integration╲  - File I/O
            ╱════════════════╲
           ╱                  ╲ Unit Tests (75%)
          ╱   Parser, Session, ╲ - SmppMessage encode/decode
         ╱    SmppConnection   ╲ - State machine transitions
        ╱════════════════════════╲ - Handler business logic
```

### 7.2 Test Organization

```
tests/
├── unit/
│   ├─ test_smpp_message.cpp       (Encode/decode)
│   ├─ test_smpp_session.cpp       (State transitions)
│   ├─ test_message_parser.cpp     (Protocol parsing)
│   ├─ test_bind_handler.cpp       (BIND logic)
│   ├─ test_ip_validator.cpp       (Whitelist checking)
│   └─ test_message_processor.cpp  (Routing)
│
├─ integration/
│   ├─ test_dbus_authenticator.cpp (D-Bus calls)
│   ├─ test_connection_lifecycle.cpp (Full connection)
│   ├─ test_concurrent_clients.cpp  (Multiple connections)
│   └─ test_configuration_reload.cpp (Config changes)
│
├─ e2e/
│   ├─ test_smpp_client.py         (Python client)
│   ├─ quick_test.py               (5-minute validation)
│   ├─ stress_test.py              (1000 concurrent)
│   └─ failure_scenarios.py         (Connection drops, etc.)
│
└─ fixtures/
    ├─ mock_dbus.cpp
    ├─ mock_tcp_socket.cpp
    └─ test_data/
        ├─ bind_transmitter_pdu.bin
        ├─ valid_credentials.conf
        └─ invalid_ips.conf
```

---

## Part 8: Phases & Dependencies

### Phase 1: Foundation (Current Work)
**Deliverables**: Working SMPP server with BIND/UNBIND/ENQUIRE_LINK
- SmppServer accepts connections
- SmppConnection processes messages
- SmppSession manages state
- D-Bus authentication integration
- Basic logging

**Blockers**: Fix smppcxx integration (see CURRENT_STATUS.md)

### Phase 2: Message Operations
**Deliverables**: SUBMIT_SM (send message), DELIVER_SM (receive)
- Depends on: Phase 1 foundation complete
- New handlers: SubmitSmHandler, DeliverSmHandler
- Message queuing/storage
- Message retry logic

### Phase 3: Monitoring & Observability
**Deliverables**: Metrics, metrics export, alerting
- Depends on: Phase 1 & 2
- Prometheus metrics endpoint
- Grafana dashboard
- Alert rules (connection drops, auth failures)

### Phase 4: HA/Clustering
**Deliverables**: Multiple servers, failover
- Depends on: Phases 1-3
- Load balancer
- Shared credential store (DB)
- Replication/sync

---

## Part 9: Error Handling Strategy

### Failure Scenarios & Recovery

| Scenario | Detection | Recovery | Logging |
|---|---|---|---|
| **Client disconnects** | socket.read() returns 0 | Close connection, cleanup session | INFO: "Client disconnected" |
| **Invalid BIND** | Parsing fails | Send error response, keep socket open | WARN: "Invalid BIND PDU" |
| **Auth fails** | D-Bus returns false | Send BIND_RESP with error status | WARN: "Auth failed for user X" |
| **D-Bus unavailable** | D-Bus call times out | Send BIND_RESP error, retry later | ERROR: "D-Bus unavailable" |
| **Server overloaded** | Connection queue full | Reject new connections gracefully | ERROR: "Max connections reached" |
| **Corrupted PDU** | Parser validation fails | Close connection (protocol violation) | ERROR: "Protocol violation" |

---

## Part 10: Non-Functional Requirements Implementation

| Requirement | How Achieved |
|---|---|
| **Concurrent Connections** | Async I/O (ASIO), one SmppConnection per client |
| **Message Throughput** | Event loop, non-blocking I/O, connection pooling |
| **Low Latency** | No synchronous D-Bus calls (async if possible) |
| **Uptime** | Graceful shutdown, connection recovery, restart policy |
| **Monitoring** | Prometheus metrics, structured logging |
| **Security** | IP whitelist, credential hashing, audit log |

---

## Part 11: Deployment Model

### Docker Compose Stack
```yaml
services:
  smsc-dev:
    build: ./Docker/smsc-dev
    ports:
      - "2775:2775"      # SMPP
      - "2222:22"        # SSH
    volumes:
      - ./:/workspace    # Source
      - /var/log/...     # Logs
      - /etc/...         # Config
    environment:
      - LOG_LEVEL=info
      - MAX_CONN=1000
    networks:
      - smpp-net
      
networks:
  smpp-net:
    driver: bridge
```

### systemd Service (Production)
```ini
[Unit]
Description=Simple SMPP Server
After=network.target dbus.service

[Service]
Type=simple
ExecStart=/usr/local/bin/simple_smpp_server
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

---

## Part 12: Interface Contracts (API Boundaries)

### SMPPServer D-Bus Interfaces

```xml
<interface name="com.telecom.SMPPServer">
  <!-- Status queries -->
  <method name="GetConnectionCount">
    <arg type="u" direction="out" name="count"/>
  </method>
  
  <method name="GetConnections">
    <arg type="a(ssstt)" direction="out" name="connections"/>
    <!-- (session_id, client_ip, state, connected_at, last_activity) -->
  </method>
  
  <!-- Control -->
  <method name="Shutdown">
    <arg type="b" direction="out" name="success"/>
  </method>
</interface>
```

### Configuration Schema

```
allowed_ips.conf:
  127.0.0.1
  ::1
  192.168.1.0/24
  10.0.0.0/8

credentials.conf:
  # username:hash:salt
  test:$2b$12$abc...:salt123
```

---

## Conclusion: Design Principles

**This system is designed around**:
1. **Separation of Concerns** - Each component has one job
2. **Dependency Injection** - Easy to test, swap implementations
3. **Fail-Safe Defaults** - Reject unknown, validate first
4. **Observability** - Log everything, make it queryable
5. **Scalability** - Async I/O, non-blocking, connection pooling
6. **Testability** - Unit testable components, integration test hooks

**The architecture is ready for**:
- Adding new SMPP commands (new Handlers)
- Scaling to multiple servers (shared credential store, load balancer)
- Monitoring integration (metrics export)
- Security hardening (rate limiting, DDoS protection)

---

**Document Status**: Complete - Ready for Review  
**Next Step**: Implement Phase 1 using this architecture  
**Owner**: Architecture/Platform Team  
**Review Date**: 2026-04-20
