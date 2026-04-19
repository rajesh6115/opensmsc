# SMPPServer Architecture Update - v2.0

**Date**: 2026-04-20  
**Title**: Process-Per-Client Microservices Architecture  
**Status**: Replaces v1.0 monolithic design  

---

## Overview

The SMPPServer architecture has been redesigned to use a **process-per-client microservices model** with D-Bus IPC, replacing the earlier monolithic single-process design.

This document explains:
1. What changed and why
2. How class responsibilities shifted
3. Impact on implementation
4. New dependencies (D-Bus)

---

## V1.0 vs V2.0 Comparison

### V1.0: Monolithic (Thread-Per-Client)

```
Single SmppServer Process
├─ TcpServer (accepts connections)
├─ SmppConnection[client1] (per-thread)
│  ├─ SmppSession
│  ├─ SmppMessageParser
│  └─ SmppMessageProcessor
├─ SmppConnection[client2] (per-thread)
│  └─ [same as above]
└─ Shared Services
   ├─ DBusAuthenticator
   ├─ CredentialValidator
   └─ Logger
```

**Problems**:
- Crash in one thread kills entire server
- Complex thread synchronization
- Shared mutable state
- Difficult resource limits per client

---

### V2.0: Microservices (Process-Per-Client)

```
SMPPServer Process (1x)        SmppClientHandler (N×, 1 per client)
├─ TcpServer                   ├─ SocketReceiver (claim socket from parent)
├─ IpValidator                 ├─ SmppConnection
├─ ProcessSpawner              │  ├─ SmppSession
└─ SocketTransfer (D-Bus)      │  ├─ SmppMessageParser
                               │  └─ SmppMessageProcessor
                               ├─ DBusAuthenticator (call service)
                               └─ spdlog (local logging)

Authenticator (1x, D-Bus Service)
└─ Authenticate(username, pwd) → bool
```

**Benefits**:
- ✅ Crash in handler doesn't affect server
- ✅ Natural process isolation
- ✅ Per-process resource limits
- ✅ Graceful handler upgrade
- ✅ Simple SMPPServer code

---

## Class Reorganization

### Classes Moving Out of SMPPServer

These classes are **REMOVED** from SMPPServer and moved to SmppClientHandler:

| Class | Reason | New Location |
|-------|--------|--------------|
| `SmppConnection` | Client handling → handler process | SmppClientHandler |
| `SmppMessageParser` | Protocol handling → handler | SmppClientHandler |
| `SmppMessageProcessor` | Request routing → handler | SmppClientHandler |
| `SmppSession` | Per-client state → handler | SmppClientHandler |
| `BindHandler` | SMPP logic → handler | SmppClientHandler |
| `UnbindHandler` | SMPP logic → handler | SmppClientHandler |
| `EnquireLinkHandler` | SMPP logic → handler | SmppClientHandler |
| `SessionManager` | Per-handler state mgmt → handler | SmppClientHandler |
| `CredentialValidator` | Only in handlers (D-Bus auth) | SmppClientHandler (via D-Bus) |
| `Logger` | spdlog library (not class) | Both (via library) |

### Classes Staying in SMPPServer

These classes remain and are **SIMPLIFIED**:

| Class | Change | Reason |
|-------|--------|--------|
| `SmppServer` | Still main orchestrator | ✓ No change |
| `TcpServer` | Only socket I/O | Removed protocol handling |
| `IpValidator` | Still validates IPs | ✓ No change |
| `DBusAuthenticator` | Only in handlers now | Moved to SmppClientHandler |

### New Classes in SMPPServer

| Class | Purpose |
|-------|---------|
| `ProcessSpawner` | Spawn `SmppClientHandler <port>` |
| `SocketTransfer` | D-Bus socket FD handoff |

### New Classes in SmppClientHandler

| Class | Purpose |
|-------|---------|
| `SocketReceiver` | Receive socket via D-Bus ClaimSocket |
| `SmppConnection` | Moved from SMPPServer |

---

## SMPPServer Executable - Simplified Design

### Responsibilities

**ONLY these tasks**:
1. Listen on TCP port 2775
2. Accept incoming connections
3. Validate client IP (whitelist)
4. Spawn SmppClientHandler with port number
5. Provide D-Bus `ClaimSocket(port)` method
6. Transfer socket FD to handler
7. Close socket locally

### Class Diagram

```
SMPPServer (main)
  ├─ TcpServer
  │  └─ ASIO socket management
  ├─ IpValidator
  │  └─ Check allowed_ips.conf
  ├─ ProcessSpawner
  │  └─ exec(/usr/local/bin/smpp_client_handler <port>)
  └─ D-Bus Service com.opensmsc.SMPPServer
     └─ Method: ClaimSocket(port: uint16) → (fd: handle, success: bool)
```

### Pseudocode

```cpp
class SmppServer {
    void start() {
        tcp_server.listen(2775);
        while (running) {
            client_socket = tcp_server.accept();
            client_ip = client_socket.remote_ip();
            client_port = client_socket.remote_port();
            
            if (ip_validator.is_allowed(client_ip)) {
                process_spawner.spawn("SmppClientHandler", client_port);
                pending_sockets[client_port] = client_socket;
            } else {
                client_socket.close();
            }
        }
    }
    
    handle_dbus_claim_socket(uint16_t port) {
        socket_fd = pending_sockets[port];
        dbus_transfer_fd(socket_fd);
        pending_sockets.erase(port);
        close_socket_locally(socket_fd);
    }
};
```

---

## SmppClientHandler Executable - New Process

### Startup Flow

```
SmppClientHandler <port>
  ├─ Parse command-line argument: port = 54321
  ├─ Connect to D-Bus system bus
  ├─ Call SMPPServer.ClaimSocket(54321)
  ├─ Receive socket FD
  ├─ Create SmppConnection(socket_fd)
  ├─ Start event loop
  └─ Handle SMPP protocol
```

### Responsibilities

**Everything protocol-related**:
1. Receive socket from parent via D-Bus
2. Parse incoming SMPP PDUs
3. Validate sequence numbers, states
4. Call Authenticator for BIND requests
5. Build and send SMPP responses
6. Manage session state machine
7. Log events via spdlog
8. Handle client disconnection
9. Clean up and exit

### Class Diagram

```
SmppClientHandler (main)
  ├─ SocketReceiver (D-Bus)
  │  └─ ClaimSocket(port) → socket_fd
  ├─ SmppConnection
  │  ├─ SmppSession (state machine)
  │  ├─ SmppMessageParser (PDU → Message)
  │  ├─ SmppMessageProcessor (router)
  │  │  ├─ BindHandler
  │  │  ├─ UnbindHandler
  │  │  └─ EnquireLinkHandler
  │  └─ SmppMessageEncoder (Message → PDU)
  ├─ DBusAuthenticator
  │  └─ Call: Authenticator.Authenticate(user, pass)
  └─ spdlog (logging library)
```

### Pseudocode

```cpp
int main(int argc, char* argv[]) {
    uint16_t port = std::stoi(argv[1]);  // e.g., 54321
    
    DBusConnection dbus;
    int socket_fd = dbus.call(
        "com.opensmsc.SMPPServer",
        "/com/opensmsc/SMPPServer",
        "ClaimSocket",
        port
    );  // Returns file descriptor
    
    SmppConnection conn(socket_fd);
    conn.run_event_loop();
    
    return 0;
}
```

---

## D-Bus Interfaces

### SMPPServer D-Bus Interface

```xml
<interface name="com.opensmsc.SMPPServer">
  <method name="ClaimSocket">
    <arg type="u" direction="in" name="client_port"/>
    <arg type="h" direction="out" name="socket_fd"/>
    <arg type="b" direction="out" name="success"/>
  </method>
</interface>
```

**Called by**: SmppClientHandler  
**Provides**: Socket FD transfer

---

### Authenticator D-Bus Interface

```xml
<interface name="com.opensmsc.SMPPAuthenticator">
  <method name="Authenticate">
    <arg type="s" direction="in" name="username"/>
    <arg type="s" direction="in" name="password"/>
    <arg type="b" direction="out" name="authenticated"/>
    <arg type="u" direction="out" name="error_code"/>
    <arg type="s" direction="out" name="message"/>
  </method>
</interface>
```

**Called by**: SmppClientHandler  
**Provides**: Credential validation

---

## Implementation Impact

### What Gets Simpler

✅ **SMPPServer code**
- 300-400 lines total
- No protocol logic
- No session management
- No message handling
- Just TCP I/O + dispatcher

✅ **Testing**
- Test server in isolation
- Inject fake clients
- No protocol complexity

✅ **Debugging**
- Server crashes won't hide issues
- Client handler crashes isolated
- Process dumps show exactly what failed

### What Gets More Complex

⚠️ **D-Bus integration**
- System dependency
- Socket FD passing requires D-Bus
- Service registration needed
- Error handling for IPC failures

⚠️ **SmppClientHandler startup**
- Parse command-line args
- D-Bus connection setup
- Socket FD reception
- Error handling if socket transfer fails

⚠️ **Deployment**
- SMPPServer executable
- SmppClientHandler executable (must be in PATH)
- D-Bus service files
- systemd service files
- Authenticator service running separately

---

## File Structure

### Old (v1.0) Structure
```
SMPPServer/
└── include/
    ├── tcp_server.hpp
    ├── smpp_connection.hpp
    ├── smpp_session.hpp
    ├── smpp_handler.hpp
    ├── smpp_message.hpp
    └── ...
```

### New (v2.0) Structure
```
SMPPServer/
├── include/
│  ├── tcp_server.hpp
│  ├── ip_validator.hpp
│  ├── process_spawner.hpp
│  └── socket_transfer.hpp
└── src/
   ├── main.cpp (SMPPServer executable)
   └── ...

SmppClientHandler/
├── include/
│  ├── socket_receiver.hpp
│  ├── smpp_connection.hpp
│  ├── smpp_session.hpp
│  ├── smpp_handler.hpp
│  ├── smpp_message.hpp
│  └── ...
└── src/
   ├── main.cpp (SmppClientHandler executable)
   └── ...
```

---

## Migration Path

### From v1.0 to v2.0

**Step 1**: Extract protocol classes
- Copy SmppMessage, Parser, Encoder → SmppClientHandler folder
- Copy SmppSession, Handlers → SmppClientHandler folder

**Step 2**: Keep network classes
- Keep TcpServer, IpValidator in SMPPServer
- Add ProcessSpawner, SocketTransfer

**Step 3**: Add D-Bus layer
- Implement ClaimSocket in SMPPServer
- Implement SocketReceiver in SmppClientHandler
- Update Authenticator to D-Bus service

**Step 4**: Build both executables
- `simple_smpp_server` (SMPPServer)
- `smpp_client_handler` (SmppClientHandler)

---

## Testing Strategy

### Unit Tests

**SMPPServer tests**:
- TcpServer accepts connections
- IpValidator checks IPs correctly
- ProcessSpawner executes correctly
- SocketTransfer passes FD correctly

**SmppClientHandler tests**:
- SocketReceiver claims socket via D-Bus
- SmppMessageParser decodes PDUs
- SmppSession state transitions work
- Handlers invoke Authenticator
- Messages encode correctly

### Integration Tests

**End-to-end**:
1. Start SMPPServer
2. Client connects to :2775
3. SMPPServer spawns SmppClientHandler
4. SmppClientHandler claims socket
5. Client sends BIND
6. SmppClientHandler parses BIND
7. Calls Authenticator via D-Bus
8. Sends response
9. Client disconnects
10. SmppClientHandler exits

---

## Performance Considerations

### Process Overhead

- Per-client process: ~10-30 MB RAM (lightweight)
- Process spawn: ~10-50 ms (acceptable)
- D-Bus call: ~1-5 ms (fast for IPC)
- Socket handoff: <1 ms (zero-copy FD passing)

### Scalability

- Support 1000+ concurrent clients
- Each handler is independent
- Server resource usage minimal
- Scale by adding more handler processes

---

## Future Enhancements

### Phase 2+

- Implement more SMPP commands (SUBMIT_SM, DELIVER_SM)
- Add message queuing
- Implement metrics/monitoring
- Add rate limiting per handler
- Support for clustering/load balancing

---

**Document Status**: Complete  
**Last Updated**: 2026-04-20  
**Next Step**: Update class_specifications/ to reflect new classes
