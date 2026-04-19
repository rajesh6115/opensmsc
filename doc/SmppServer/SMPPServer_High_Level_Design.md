# SMPPServer High-Level Design

**Date**: 2026-04-19  
**Version**: 1.0  
**Scope**: SMPPServer component (SMPP protocol + client connection handling)  
**Purpose**: Define components and communication interfaces at a glance

---

## Component Overview

```
┌─────────────────────────────────────┐
│         SmppServer                  │
│     (Orchestrator, main())          │
└──────────────┬──────────────────────┘
               │
               ├─ Creates & starts
               ↓
        ┌─────────────────────┐
        │    TcpServer        │
        │  (ASIO acceptor)    │
        │ :2775/IPv6+IPv4     │
        └────────┬────────────┘
                 │
                 ├─ Accepts connection
                 ├─ Validates IP (IpValidator)
                 ├─ Creates new instance
                 ↓
          ┌────────────────────────┐
          │  SmppConnection        │
          │ (per-client instance)  │
          │ async I/O             │
          └────────┬───────────────┘
                   │
                   ├─ Reads TCP bytes
                   │
                   ├─ Protocol Bridge (SmppProtocolAdapter)
                   │  ├─ Buffers TCP data
                   │  ├─ Detects complete PDU
                   │  └─ Converts bytes → smppcxx objects
                   │
                   ├─ smppcxx Library
                   │  ├─ Smpp::Header, Smpp::BindReceiver, etc. (typed PDU objects)
                   │  └─ encode(), decode() for serialization
                   │
                   ├─ Tracks connection state
                   │  (SmppSession per connection)
                   │
                   ├─ Routes message to handler
                   │  (SmppMessageProcessor)
                   │
                   └─ Handlers perform auth, state changes
                      (5 concrete: BindTx, BindRx, BindTrx, Unbind, EnquireLink)
                      ├─ Work with smppcxx typed objects
                      ├─ Call CredentialValidator (checks IP/user)
                      ├─ Call DBusAuthenticator (validates password via D-Bus)
                      └─ Call Logger (spdlog)
```

---

## Components & Responsibilities

| Component | Purpose | Owns |
|---|---|---|
| **SmppServer** | Startup orchestrator, dependency injection | main() logic, service initialization |
| **TcpServer** | ASIO acceptor, connection acceptance | Listening socket, IP validation before accept |
| **SmppConnection** | Per-client async I/O state machine | Network reads/writes, PDU buffering, calls SmppProtocolAdapter |
| **SmppSession** | Client state tracker | UNBOUND/BOUND_TX/RX/TRX state machine, validated username |
| **SmppProtocolAdapter** | TCP ↔ smppcxx bridge | Streaming PDU buffering, type-conversion from bytes → smppcxx objects, encoding responses |
| **smppcxx Library** | SMPP Protocol implementation | Smpp::Header, Smpp::BindReceiver, Smpp::BindTransmitter, etc. (typed PDU objects) |
| **SmppMessageProcessor** | Message router & orchestrator | Dynamic_cast smppcxx::Header* to specific type, dispatch to correct handler |
| **BindTransmitterHandler** | BIND_TRANSMITTER logic | Extract from Smpp::BindTransmitter, authenticate, build Smpp::BindTransmitterResp |
| **BindReceiverHandler** | BIND_RECEIVER logic | Extract from Smpp::BindReceiver, authenticate, build Smpp::BindReceiverResp |
| **BindTransceiverHandler** | BIND_TRANSCEIVER logic | Extract from Smpp::BindTransceiver, authenticate, build Smpp::BindTransceiverResp |
| **UnbindHandler** | UNBIND logic | Validate bound state, transition to UNBOUND, build Smpp::UnbindResp |
| **EnquireLinkHandler** | ENQUIRE_LINK (keep-alive) | Validate bound state, build Smpp::EnquireLinkResp |
| **IpValidator** | IP whitelist enforcement | Check incoming connection IP before accept |
| **CredentialValidator** | Credential validation rules | Check if username/password format valid |
| **DBusAuthenticator** | D-Bus credential checker | Call SMPPAuthenticator service over D-Bus |
| **Logger** | spdlog wrapper | Initialize sinks, expose LOG_* macros |
| **SessionManager** | Active session tracking | Map session_id → SmppSession, cleanup on disconnect |

---

## Communication Patterns

### 1. **Constructor Injection** (all classes)
All classes receive dependencies via constructor, not created internally:
```
SmppServer creates TcpServer, passes:
  - IpValidator
  - Logger
  - DBusAuthenticator
  - CredentialValidator

SmppConnection receives:
  - SmppSession (per client)
  - SmppMessageParser
  - SmppMessageProcessor

SmppMessageProcessor receives:
  - CredentialValidator
  - DBusAuthenticator
  - Logger
  - All 5 handlers (Bind, Unbind, EnquireLink)
```

### 2. **ASIO Async Chains** (TcpServer ↔ SmppConnection)
```
TcpServer::start()
  ├─ async_accept()
  │  └─ on_accept(client_socket, ip)
  │     ├─ validate IP (IpValidator)
  │     └─ create SmppConnection(socket, ...)
  │        └─ async_read_pdu_header()
  │           └─ on_header_read()
  │              └─ async_read_pdu_body()
  │                 └─ on_body_read()
  │                    ├─ Assemble full_pdu
  │                    └─ SmppHandler::dispatch_pdu(full_pdu, session)
```

### 3. **Message Processing** (SmppConnection → SmppMessageProcessor → Handlers)
```
dispatch_pdu(full_pdu, session)
  ├─ Parse full_pdu → SmppMessage
  ├─ SmppMessageProcessor::process_message(msg, session)
  │  ├─ Switch on command_id
  │  ├─ Call appropriate handler
  │  │  └─ Handler::handle(msg, session)
  │  │     ├─ Validate session state
  │  │     ├─ Extract credentials (if BIND)
  │  │     ├─ DBusAuthenticator::validate(user, pass) → bool
  │  │     ├─ session.try_bind_as_*() → bool
  │  │     └─ Return response SmppMessage
  │  └─ Return response
  └─ Encode response → bytes
  └─ async_write() response to client
```

### 4. **State Machine** (SmppSession)
```
UNBOUND
  ├─ try_bind_as_transmitter(user) → BOUND_TRANSMITTER
  ├─ try_bind_as_receiver(user) → BOUND_RECEIVER
  └─ try_bind_as_transceiver(user) → BOUND_TRANSCEIVER

BOUND_* (any)
  ├─ try_unbind() → UNBOUND
  ├─ touch() [track activity for idle detection]
  └─ queries: is_bound(), can_transmit(), can_receive()
```

### 5. **D-Bus Communication** (SMPPServer ↔ SMPPAuthenticator)
```
DBusAuthenticator::validate(username, password)
  → D-Bus method call to /com/telecom/SMPPAuthenticator
  → SMPPAuthenticator::Authenticate(user, pass, ip)
  → Returns: (success: bool, message: string)
```

### 6. **Logging** (all classes)
All logging via Logger spdlog wrapper:
```
Logger::init(path, level)
  ├─ Create stdout_color_sink_mt (colored console)
  └─ Create rotating_file_sink_mt (/var/log/simple_smpp_server/server.log)
  └─ Set pattern: "[%Y-%m-%d %H:%M:%S.%e] [%^%-5l%$] %v"
  └─ Set level: DEBUG|INFO|WARN|ERROR

Everywhere: LOG_INFO("Module", "message: {}", val)
```

---

## Data Flow Example: BIND_TRANSMITTER Success

```
Client connects
  ↓
TcpServer::async_accept()
  ├─ Extract client IP
  ├─ IpValidator::is_whitelisted(ip) → true
  └─ Create SmppConnection(socket, session, parser, processor)
  
Client sends BIND_TRANSMITTER PDU
  ↓
SmppConnection::async_read_pdu_header()
  └─ Read 16 bytes: command_length=48, command_id=0x00000002
  
SmppConnection::async_read_pdu_body()
  └─ Read 32 bytes (48 - 16) into body_buffer
  
SmppConnection::on_body_read()
  ├─ Combine header + body into full_pdu
  └─ SmppHandler::dispatch_pdu(full_pdu, session)
  
SmppMessageProcessor::process_message()
  ├─ Identify command_id = BIND_TRANSMITTER
  └─ Call BindTransmitterHandler::handle(msg, session)
  
BindTransmitterHandler::handle()
  ├─ Extract system_id="test", password="pass" from msg body
  ├─ DBusAuthenticator::validate("test", "pass", client_ip)
  │  └─ D-Bus RPC → SMPPAuthenticator.Authenticate()
  │     └─ Returns: success=true
  ├─ session.try_bind_as_transmitter("test")
  │  ├─ Check state == UNBOUND → true
  │  └─ Set state = BOUND_TRANSMITTER, authenticated_as = "test"
  └─ Return SmppMessage(status=0, body="TestServer\0")
  
SmppConnection::on_body_read() [continued]
  ├─ Logger::info("BIND_TRANSMITTER successful, user=test")
  └─ async_write(response PDU to client)
```

---

## Key Design Decisions

1. **SmppMessage is a value object** — immutable, copyable, no dependencies
2. **SmppMessageParser owns streaming state** — handles partial PDU buffering
3. **SmppSession owns connection state** — prevents invalid state transitions
4. **Handlers are stateless and reusable** — all state in SmppSession
5. **All async operations via ASIO callbacks** — non-blocking, single-threaded event loop (or thread pool)
6. **Dependency injection via constructors** — testable, no global state
7. **Logging is a service, not a mechanism** — injected, not instantiated

---

## Design Patterns Used

| Pattern | Where | Why |
|---|---|---|
| **Factory** | SmppServer::create() | Manage object lifetime, DI container |
| **Strategy** | SmppRequestHandler + 5 handlers | Dispatch handler per command_id |
| **State Machine** | SmppSession | Prevent invalid state transitions (BIND twice) |
| **Value Object** | SmppMessage | Immutable, thread-safe, testable |
| **Adapter** | DBusAuthenticator | Abstract D-Bus details from handlers |
| **Template Method** | ASIO async callbacks | Chain reads: header → body → process |

---

## Scalability & Performance Considerations

- **ASIO async**: Non-blocking I/O, single thread can handle 1000+ connections
- **SmppMessage immutable**: No synchronization needed when passing between handlers
- **SmppSession per client**: Each SmppConnection owns its session, no shared mutable state
- **Stateless handlers**: Can be reused, no memory per connection
- **Logging sinks**: Rotating file + async spdlog, doesn't block main loop

---

**Next**: Detailed class specifications in `class_specifications/`
