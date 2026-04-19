# Sequence Diagrams: SMPPServer Flow

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: Visualize class interactions for main SMPP operations  

---

## Part 1: System Startup Sequence

### Initialization Flow

```
main()
  ├─ Create Logger
  │   └─ Initialize spdlog (file + console sinks)
  │
  ├─ Create IpValidator
  │   └─ Load /etc/simple_smpp_server/allowed_ips.conf
  │
  ├─ Create CredentialValidator
  │   └─ Load /etc/simple_smpp_server/credentials.conf
  │
  ├─ Create DBusAuthenticator
  │   └─ Connect to system D-Bus
  │
  ├─ Create SessionManager
  │   └─ Initialize empty session map
  │
  ├─ Create TcpServer
  │   └─ Create asio::ip::tcp::acceptor on port 2775
  │
  ├─ Create SmppServer
  │   └─ Inject all dependencies
  │
  └─ Call SmppServer::start()
      └─ Begin async accept loop
```

---

## Part 2: Connection Acceptance

```
┌─────────────────────────────────────────────────────────────┐
│ SEQUENCE: Client Connects (TCP Level)                       │
└─────────────────────────────────────────────────────────────┘

Client                TcpServer              IpValidator
  │                       │                       │
  ├─ TCP CONNECT ─────→  │                       │
  │                       │                       │
  │                   [socket accepted]          │
  │                       │                       │
  │                       ├─ get_remote_ip()     │
  │                       │                       │
  │                       ├─ is_allowed(ip) ──→ │
  │                       │                       │
  │                       │ [check whitelist]    │
  │                       │←────── true ────────│
  │                       │                       │
  │                   [IP validated]
  │                       │
  │                   [Create SmppConnection]
  │                       │
  │                   [Start async_read]
  │
  │ (Waiting for BIND)
```

**Key Points**:
- IP validation happens BEFORE creating SmppConnection
- If IP not in whitelist → close socket immediately
- Each accepted socket gets its own SmppConnection instance

---

## Part 3: BIND_TRANSMITTER Flow (Main Use Case)

```
┌──────────────────────────────────────────────────────────────────┐
│ SEQUENCE: BIND_TRANSMITTER Request (Full Flow)                   │
└──────────────────────────────────────────────────────────────────┘

Client      Socket      SmppConnection    SmppMessageParser
  │           │              │                    │
  ├─ BIND ───→│              │                    │
  │           │    [async_read fires]            │
  │           │              │                    │
  │           │          ├─ feed_bytes ─────────→│
  │           │          │                        │
  │           │          │ [parse header + body] │
  │           │          │←─ SmppMessage ────────│
  │           │          │                        │
  │           │      [SmppConnection::on_socket_data]
  │           │          │
  │           │      [process_message(msg)]
  │           │
```

Continue with message processing:

```
SmppConnection    SmppMessageProcessor    SmppSession
    │                    │                    │
├─ process_msg ────────→ │                    │
│                    ├─ get_state()          │
│                    │                        │
│                    │   [state = UNBOUND]   │
│                    │                        │
│                    ├─ is_bind_transmitter() = true
│                    │
│                    ├─ extract_system_id() = "test"
│                    ├─ extract_password() = "test"
│                    │
│                    ├─ validate_credentials()
│                    │   ├─ Check if "test/test" valid
│                    │   └─ return true
│                    │
│                    ├─ try_bind_as_transmitter("test")
│                    │         ├─ Check state = UNBOUND ✓
│                    │         ├─ set state = BOUND_TRANSMITTER
│                    │         ├─ set authenticated_as = "test"
│                    │         └─ return true ✓
│                    │
│                    ├─ Build response
│                    │   └─ BIND_TRANSMITTER_RESP (status=0)
│                    │
│                    ├─ return response message
│                    │
├─ encode(response)
│
├─ socket.write(bytes)
│
└─ async_read (next message)
│
└─────→ BIND_TRANSMITTER_RESP sent to client
```

**Full Diagram with All Actors**:

```
Client     SmppConn   Parser    Processor   Session   DBusAuth   Logger
  │           │         │          │          │         │          │
  ├─BIND─────→│         │          │          │         │          │
  │           │         │          │          │         │          │
  │           ├─feed────→│         │          │         │          │
  │           │         ├─parse──→ SmppMsg   │         │          │
  │           │←────────────────────         │         │          │
  │           │         │          │         │         │          │
  │           │         │          ├─check_state()     │          │
  │           │         │          ├─extract_creds()   │          │
  │           │         │          │         │         │          │
  │           │         │          ├─auth──────────────│          │
  │           │         │          │    (D-Bus call)   │          │
  │           │         │          │←────────true      │          │
  │           │         │          │         │         │          │
  │           │         │          ├─bind────→│        │          │
  │           │         │          │   username       │          │
  │           │         │          │←─true────│        │          │
  │           │         │          │         │         │          │
  │           │         │          ├─build_response()  │          │
  │           │         │          │←─BIND_RESP       │          │
  │           │         │          │         │         │          │
  │           ├─log────────────────────────────────────┤──────────→│
  │           │ (BIND successful)                      │          │
  │           │         │          │         │         │          │
  │           ├─encode()
  │           │
  │           ├─socket.write()
  │           │
  │←─BIND_RESP───────────│
  │           │         │
  │           ├─async_read (next msg)
  │           │
```

---

## Part 4: ENQUIRE_LINK Flow (Keep-Alive)

```
┌──────────────────────────────────────────────────────────┐
│ SEQUENCE: ENQUIRE_LINK (Keep-Alive)                      │
└──────────────────────────────────────────────────────────┘

Client      SmppConnection    SmppMessageProcessor    SmppSession
  │              │                     │                   │
  ├─ENQUIRE_LINK→│                     │                   │
  │              │                     │                   │
  │              ├─process_message────→│                   │
  │              │                ├─check_state()         │
  │              │                │       ├─is_bound()   │
  │              │                │       └─ true ✓      │
  │              │                │                       │
  │              │                ├─is_enquire_link()     │
  │              │                │       └─ true ✓      │
  │              │                │                       │
  │              │                ├─build_response()      │
  │              │                │  ENQUIRE_LINK_RESP    │
  │              │                │  (status=0, seq=N)    │
  │              │                │                       │
  │              │←────────response message───────        │
  │              │                │                       │
  │              ├─encode()
  │              │
  │              ├─socket.write()
  │              │
  │←─ENQUIRE_LINK_RESP─────────────│
  │              │
  │         [Ready for next msg]
```

**Key Points**:
- ENQUIRE_LINK requires session to be BOUND
- Returns immediately (no auth needed)
- Used for keep-alive/connection validation

---

## Part 5: UNBIND Flow (Graceful Disconnect)

```
┌──────────────────────────────────────────────────────┐
│ SEQUENCE: UNBIND (Graceful Close)                    │
└──────────────────────────────────────────────────────┘

Client      SmppConnection    SmppMessageProcessor    SmppSession
  │              │                     │                   │
  ├─UNBIND──────→│                     │                   │
  │              │                     │                   │
  │              ├─process_message────→│                   │
  │              │                ├─check_state()         │
  │              │                │       ├─is_bound()   │
  │              │                │       └─ true ✓      │
  │              │                │                       │
  │              │                ├─is_unbind()          │
  │              │                │       └─ true ✓      │
  │              │                │                       │
  │              │                ├─try_unbind()────────→│
  │              │                │   ├─set state=UNBOUND│
  │              │                │   └─return true      │
  │              │                │←───────────────      │
  │              │                │                       │
  │              │                ├─build_response()      │
  │              │                │  UNBIND_RESP          │
  │              │                │  (status=0, seq=N)    │
  │              │                │                       │
  │              │←────────response message───────        │
  │              │                │                       │
  │              ├─encode()
  │              │
  │              ├─socket.write()
  │              │
  │←─UNBIND_RESP──────────────────│
  │              │
  │              ├─socket.close()
  │              │
  │              ├─cleanup session
  │              │
```

**Key Points**:
- Send UNBIND_RESP before closing socket
- Close connection after response sent
- Cleanup session resources

---

## Part 6: Error Case - Invalid Credentials

```
┌──────────────────────────────────────────────────────────┐
│ SEQUENCE: BIND with Invalid Credentials                  │
└──────────────────────────────────────────────────────────┘

Client         SmppConnection    Processor       DBusAuth    Session
  │                  │               │              │          │
  ├─BIND─────────────→│               │              │          │
  │             (user="test",          │              │          │
  │              pass="wrong")         │              │          │
  │                  │               │              │          │
  │                  ├─process───────→│              │          │
  │                  │            ├─extract_creds() │          │
  │                  │            ├─auth(test,wrong)─│        │
  │                  │            │         [D-Bus]  │        │
  │                  │            │                  │        │
  │                  │            │  [Check creds]  │        │
  │                  │            │←─────false─────│        │
  │                  │            │              │          │
  │                  │            ├─build_error_resp()
  │                  │            │   status=0x0E (ESME_RINVPASWD)
  │                  │            │   seq=1
  │                  │            │
  │                  │←───response message (error)───
  │                  │
  │                  ├─socket.close()
  │                  │
  │                  ├─cleanup
  │                  │
  │←─BIND_RESP(error)
  │ (Connection closed)
```

**Key Points**:
- Auth check fails via D-Bus
- Send error status code (ESME_RINVPASWD = 0x0E)
- Close connection after error response
- Session never transitions to BOUND state

---

## Part 7: Error Case - Already Bound (Double BIND)

```
┌──────────────────────────────────────────────────────────┐
│ SEQUENCE: Double BIND (Error Case)                       │
└──────────────────────────────────────────────────────────┘

[After first successful BIND...]

Client         SmppConnection    Processor      Session
  │                  │               │           │
  │             [state=BOUND_TX]    │           │
  │                  │               │           │
  │                  │               │    ├─BIND─────────→│
  │                  │               │           │
  │                  ├─process───────→│           │
  │                  │            ├─try_bind──→│
  │                  │            │    │        │
  │                  │            │    ├─check state=BOUND_TX
  │                  │            │    │        │
  │                  │            │    ├─invalid transition!
  │                  │            │    │        │
  │                  │            │    └─return false
  │                  │            │←──────false──
  │                  │            │
  │                  │            ├─build_error_resp()
  │                  │            │   status=0x05 (ESME_RALYBND)
  │                  │            │   "Already Bound"
  │                  │            │
  │                  │←───response message (error)───
  │                  │
  │←─BIND_RESP(error)
  │ (Connection remains open)
```

**Key Points**:
- Session state machine prevents invalid transitions
- Error code ESME_RALYBND (0x05) = "Already Bound"
- Connection stays open (client can try again if needed)
- Sequence number tracking prevents confusion

---

## Part 8: Error Case - ENQUIRE_LINK Without BIND

```
┌──────────────────────────────────────────────────────────┐
│ SEQUENCE: ENQUIRE_LINK from UNBOUND Connection           │
└──────────────────────────────────────────────────────────┘

Client         SmppConnection    Processor      Session
  │                  │               │           │
  │             [state=UNBOUND]     │           │
  │                  │               │           │
  │                  ├─process───────→│           │
  │                  │            ├─is_bound()─→│
  │                  │            │               │
  │                  │            │   [check]    │
  │                  │            │               │
  │                  │            │   false ✗    │
  │                  │            │←─────────────
  │                  │            │
  │                  │            ├─build_error_resp()
  │                  │            │   status=0x0D (ESME_RBINDFAIL)
  │                  │            │   "Not Bound"
  │                  │            │
  │                  │←───response message (error)───
  │                  │
  │←─ENQUIRE_LINK_RESP(error)
  │ (Connection remains open)
```

---

## Part 9: Multi-Client Scenario (Concurrent Connections)

```
┌──────────────────────────────────────────────────────────┐
│ SEQUENCE: Two Clients Connected Simultaneously           │
└──────────────────────────────────────────────────────────┘

                        SmppServer
                            │
              ┌─────────────┴──────────────┐
              │                            │
        SmppConnection                SmppConnection
        (Client 1)                    (Client 2)
              │                            │
          Session 1                   Session 2
          BOUND_TX                    BOUND_RX
              │                            │
         Parser 1                    Parser 2
              │                            │
      Processor (shared)              Processor (shared)
         (one instance)                   │
              │◄──────────────────────────┤
              │
        [Routes to Handler]
              │
        [Handlers are stateless]


Timeline:
T1: Client1 connects → SmppConnection#1 created → Session#1
T2: Client2 connects → SmppConnection#2 created → Session#2
T3: Client1 sends BIND_TX → routed to handler → Session#1.try_bind_as_transmitter()
T4: Client2 sends BIND_RX → routed to handler → Session#2.try_bind_as_receiver()
T5: Both sessions now bound (independently)
T6: Client1 sends ENQUIRE_LINK → handled using Session#1 state
T7: Client2 sends ENQUIRE_LINK → handled using Session#2 state
T8: Client1 closes → Session#1 destroyed
T9: Client2 still active → Session#2 remains
```

**Key Points**:
- Each client gets its own SmppConnection and SmppSession
- SessionManager tracks all sessions
- Processor, Handlers are shared/stateless
- No cross-session interference

---

## Part 10: Socket Read/Write Cycle (ASIO Async)

```
┌──────────────────────────────────────────────────────────┐
│ ASYNC I/O CYCLE (Per Client)                             │
└──────────────────────────────────────────────────────────┘

[Initial State]
    SmppConnection created
    ├─ socket attached
    ├─ call async_read(buffer, callback)
    └─ return to event loop

[Event Loop]
    ├─ Wait for I/O
    │
    ├─ Data arrives on socket
    │
    ├─ Trigger callback: on_socket_data
    │   ├─ Parse message
    │   ├─ Process message
    │   ├─ Send response
    │   └─ return
    │
    ├─ Call async_read again
    │
    ├─ Return to waiting
    │
    └─ (Repeat)

[Error/Disconnect]
    ├─ socket.read() returns error or 0
    ├─ Call socket.close()
    ├─ Cleanup SmppConnection
    └─ Return to accepting
```

**Key Points**:
- Async I/O allows thousands of concurrent connections
- One thread can handle many connections
- Never blocks on socket I/O
- ASIO handles demultiplexing

---

## Summary: Call Chain

### Incoming Message Processing:
```
socket.async_read()
  → SmppConnection::on_socket_data()
    → SmppMessageParser::parse_bytes()
      → SmppConnection::process_message()
        → SmppMessageProcessor::process_message()
          → Appropriate Handler::handle()
            → SmppSession state update
            → SmppMessageEncoder::build_response()
              → SmppConnection::send_response()
                → socket.async_write()
```

### Outgoing Response:
```
socket.async_write(encoded_bytes)
  → socket.async_read() [for next message]
```

---

**Document Status**: Complete - Ready for Implementation  
**Next Step**: Begin Phase 1 Development  
**Owner**: Development Team  
