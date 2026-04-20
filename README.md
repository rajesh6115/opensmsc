# Simple SMPP Server - Microservices Architecture

**Version**: 0.1.0 - Pre-Release (Architecture Design Phase)  
**Status**: 🟡 In Development - Architecture documented, implementation not started

> A production-grade SMSC (Short Message Service Center) built as a collection of independent microservices that communicate via D-Bus, appearing as a single unified SMSP gateway to external clients.

**SMPP Specification**: This implementation is based on the [SMPP v5.0 Specification](https://smpp.org/SMPP_v5.pdf) - The Short Message Peer-to-Peer Protocol version 5.0, which defines the protocol for exchanging SMS messages with message centers.

---

## 📌 Versioning Policy

This project follows **Semantic Versioning** (SemVer):

| Version | Status | Description |
|---------|--------|-------------|
| **0.1.0 - 0.1.x** | Pre-Release | Architecture design, no implementation yet. Only patch bumps (0.1.1, 0.1.2, etc.) |
| **1.0.0+** | Stable MVP | Minimum viable product working. Core BIND/UNBIND/ENQUIRE_LINK functional. |
| **Minor bumps** | After 1.0.0 | New features added (e.g., SUBMIT_SM, DELIVER_SM) |
| **Major bumps** | Breaking changes | API/architecture changes that break compatibility |

**Current Phase**: 0.1.0 - Architecture designed, waiting implementation to begin

---

## 🎯 Vision

Build an **SMSC that looks like a monolith from outside but is powered by microservices inside**.

```
┌────────────────────────────────────────────────────┐
│   External SMPP Clients (see single SMSC:2775)    │
└─────┬──────────────────────────┬──────────────────┘
      │ :54321                   │ :54322
      │ (TCP Port 2775)          │ (TCP Port 2775)
      ↓                          ↓
┌──────────────────────────────────────────────────────┐
│            SMPP Server (Main Process)               │
│  - Listen on port 2775                             │
│  - Accept TCP connections (note client ports)      │
│  - Validate client IP (whitelist)                  │
│  - Spawn SmppClientHandler with port number       │
│  - Hold socket until claimed via D-Bus             │
│  - Transfer socket and close locally               │
└────┬─────────────────────────────┬──────────────────┘
     │ spawn SmppClientHandler      │ spawn SmppClientHandler
     │ 54321                        │ 54322
     │                              │
     │ ClaimSocket(54321)           │ ClaimSocket(54322)
     │ (D-Bus)                      │ (D-Bus)
     ↓                              ↓
┌─────────────────────┐    ┌─────────────────────┐
│ SmppClientHandler   │    │ SmppClientHandler   │
│ (pid: nnnn)         │    │ (pid: mmmm)         │
│ port: 54321         │    │ port: 54322         │
│                     │    │                     │
│ - Parse PDUs        │    │ - Parse PDUs        │
│ - Call Authenticator│    │ - Call Authenticator│
│ - Handle protocol   │    │ - Handle protocol   │
│ - Log via spdlog    │    │ - Log via spdlog    │
└────┬────────────────┘    └────┬────────────────┘
     │                          │
     └──────────┬───────────────┘
                │ D-Bus IPC
                ↓
        ┌──────────────────┐
        │ Authenticator    │
        │ (D-Bus Service)  │
        │                  │
        │ - Validate creds │
        │ - Manage store   │
        └──────────────────┘

All processes use spdlog library for logging:
└─→ Logs written to /var/log/simple_smpp_server/*.log
```

---

## 📋 Architecture Overview

### Process-Per-Client Microservices Design

The system uses a **process-per-client** pattern where each connection gets its own handler process:

| Component | Type | Purpose |
|-----------|------|---------|
| **SMPP Server** | Main process (2775) | Accept TCP connections, validate IPs, spawn handlers |
| **SmppClientHandler** | Per-client process | Handle PDU parsing, auth, protocol logic (spawned for each client) |
| **Authenticator** | D-Bus service | Authenticate users via BIND, manage credentials |

### Connection Lifecycle

```
[Client connects to SMPPServer:2775 on client_port (e.g., :54321)]
         ↓
[SMPPServer accepts connection, notes client_port]
         ↓
[SMPPServer validates IP against whitelist]
         ↓
    ├─ IP rejected? → Close socket, exit
    └─ IP accepted? → Continue
         ↓
[SMPPServer spawns: SmppClientHandler 54321]
         ↓
[SmppClientHandler starts up with port number as argument]
         ↓
[SmppClientHandler calls D-Bus: ClaimSocket(54321)]
         ↓
[SMPPServer transfers socket FD via D-Bus]
[SMPPServer closes its end of the socket locally]
         ↓
[SmppClientHandler takes ownership of socket]
    - Parse incoming PDUs
    - Call Authenticator for BIND
    - Handle all protocol commands
    - Log events via spdlog
    - Send responses back to client
         ↓
[When client disconnects]
    - SmppClientHandler cleans up
    - Process exits
```

### How It Works

1. **Client connects** to port 2775 (SMPP Server) on ephemeral port (e.g., :54321)
2. **SMPP Server** validates client IP against whitelist
3. **SMPP Server** spawns new process: `SmppClientHandler 54321`
4. **SmppClientHandler** starts and immediately calls D-Bus: `ClaimSocket(54321)`
5. **SMPP Server** transfers socket FD via D-Bus to SmppClientHandler
6. **SMPP Server** closes its end of the socket
7. **SmppClientHandler** takes ownership of the socket
8. **Client sends BIND** request (raw bytes)
9. **SmppClientHandler** parses PDU, extracts credentials
10. **SmppClientHandler** calls **Authenticator** via D-Bus
11. **Authenticator** checks credential store, returns success/failure
12. **SmppClientHandler** sends BIND response to client
13. **SmppClientHandler** logs events via spdlog (shared library)
14. Client disconnects → SmppClientHandler process exits

---

## 🏗️ Core Components

### 1. SMPP Server (`simple_smpp_server`)

**Responsibilities**:
- Listen on TCP port 2775
- Accept incoming client connections
- Validate client IP against whitelist
- Spawn `SmppClientHandler` process for each valid client
- Transfer socket to handler via D-Bus

**Key Classes**:
```cpp
TcpServer              // Low-level socket management (ASIO)
IpValidator           // IP whitelist validation
ProcessSpawner        // Spawn SmppClientHandler binaries
SocketTransfer        // Transfer socket via D-Bus
```

**Does NOT Handle**:
- ❌ PDU parsing (handled by SmppClientHandler)
- ❌ Message routing/dispatching (handled by SmppClientHandler)
- ❌ Authentication (handled by Authenticator service)
- ❌ Protocol logic (handled by SmppClientHandler)

**Entry Point**: `SMPPServer/src/main.cpp`

---

### 2. SmppClientHandler (Per-client Binary)

**Spawned**: When a client connects AND IP is validated  
**Lifetime**: Duration of client connection  
**Process**: One new process per connected client

**Responsibilities**:
- Parse incoming SMPP PDUs
- Extract fields (username, password, sequence numbers, etc.)
- Call Authenticator service for BIND validation
- Build SMPP response messages
- Send responses back to client
- Handle all SMPP protocol commands (BIND, UNBIND, ENQUIRE_LINK, etc.)
- Log events via spdlog library
- Close connection when client disconnects

**Key Classes**:
```cpp
SmppConnection        // Client connection handler (receives socket from parent)
SmppMessageParser     // SMPP protocol parsing
SmppSession           // Per-client state management
BindHandler           // Handle BIND commands
UnbindHandler         // Handle UNBIND commands
EnquireLinkHandler    // Handle ENQUIRE_LINK commands
DbusCaller            // Call remote D-Bus services (Auth)
Logger                // spdlog-based logging (shared library)
```

**Socket Handoff**:
- Receives socket file descriptor via D-Bus from SMPPServer
- Takes full ownership of the connection
- Parent process can continue accepting new clients

**Entry Point**: `SMPPServer/src/client_handler.cpp`

### 2. Authenticator Service (`smpp_authenticator`)

**Responsibilities**:
- Expose D-Bus service `/com/telecom/SMPPAuthenticator`
- Authenticate(username, password) → bool
- Load credentials from config
- Hash passwords with salt
- Return success/failure

**Config**:
```
/etc/simple_smpp_server/credentials.conf
Format: username:password_hash:salt
```

**D-Bus Interface**:
```xml
<interface name="com.telecom.SMPPAuthenticator">
  <method name="Authenticate">
    <arg type="s" direction="in" name="username"/>
    <arg type="s" direction="in" name="password"/>
    <arg type="b" direction="out" name="success"/>
  </method>
</interface>
```

### 3. Logger Service (`smpp_logger`)

**Responsibilities**:
- Expose D-Bus service `/com/telecom/SMPPLogger`
- Accept log messages from all services
- Write to rotating log files
- Format with timestamps, levels, components
- Provide audit trail for compliance

**Log Files**:
```
/var/log/simple_smpp_server/
├── server.log          (rotating: 10MB × 5)
├── audit.log           (append-only)
└── debug.log           (verbose, rotating)
```

---

## 🔄 Message Flow Examples

Detailed sequence diagrams showing how services communicate:

### 1. Connection Establishment Flow

Shows the complete TCP handshake, IP validation, process spawning, and socket handoff via D-Bus.
![Connection Establishment Flow](http://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/rajesh6115/opensmsc/refs/heads/main/doc/diagrams/connection_establishment.puml&cache=no)

**Key Steps**:
1. Client initiates TCP connection (SYN/SYN-ACK/ACK)
2. SMPPServer validates IP against whitelist
3. SMPPServer spawns SmppClientHandler with client port
4. SmppClientHandler calls ClaimSocket(port) via D-Bus
5. SMPPServer transfers socket FD to SmppClientHandler
6. SMPPServer closes its end
7. SmppClientHandler ready to handle client

---

### 2. BIND_TRANSMITTER Authentication Flow

Shows BIND request handling with credential validation.

```plantuml
!include ./doc/diagrams/bind_transmitter_flow.puml
```

**Key Steps**:
1. Client sends BIND_TRANSMITTER PDU
2. SmppClientHandler parses PDU and extracts credentials
3. SmppClientHandler calls Authenticator service via D-Bus
4. Authenticator validates credentials against credential store
5. SmppClientHandler sets session state based on auth result
6. Event logged via spdlog
7. BIND_TRANSMITTER_RESP sent back to client

---

### 3. ENQUIRE_LINK Keep-Alive Flow

Shows keep-alive message handling for connection health checks.

```plantuml
!include ./doc/diagrams/enquire_link_flow.puml
```

**Key Steps**:
1. Client sends ENQUIRE_LINK (keep-alive)
2. SmppClientHandler immediately processes keep-alive
3. Session validation and response preparation
4. Event logged via spdlog
5. ENQUIRE_LINK_RESP sent back to client

---

## 📦 Project Structure

```
simple_smpp_server/
├── README.md                          # This file
├── CMakeLists.txt                     # Build configuration
│
├── SMPPServer/                        # Main SMPP Server Binary
│   ├── include/
│   │   ├── tcp_server.hpp
│   │   ├── ip_validator.hpp
│   │   ├── process_spawner.hpp
│   │   └── socket_transfer.hpp
│   │
│   └── src/
│       ├── main.cpp
│       ├── tcp_server.cpp
│       ├── ip_validator.cpp
│       ├── process_spawner.cpp
│       └── socket_transfer.cpp
│
├── SmppClientHandler/                 # Per-Client Handler Binary
│   ├── include/
│   │   ├── smpp_connection.hpp
│   │   ├── smpp_session.hpp
│   │   ├── smpp_message_parser.hpp
│   │   ├── smpp_handler.hpp
│   │   ├── bind_handler.hpp
│   │   ├── unbind_handler.hpp
│   │   ├── enquire_link_handler.hpp
│   │   ├── dbus_caller.hpp
│   │   └── socket_receiver.hpp
│   │
│   └── src/
│       ├── main.cpp
│       ├── smpp_connection.cpp
│       ├── smpp_session.cpp
│       ├── smpp_message_parser.cpp
│       ├── smpp_handler.cpp
│       ├── bind_handler.cpp
│       ├── unbind_handler.cpp
│       ├── enquire_link_handler.cpp
│       ├── dbus_caller.cpp
│       └── socket_receiver.cpp
│
├── Authenticator/                     # (Phase 2) Authenticator Service
│   ├── include/
│   │   ├── dbus_authenticator.hpp
│   │   └── credential_store.hpp
│   └── src/
│       ├── main.cpp
│       ├── dbus_authenticator.cpp
│       └── credential_store.cpp
│
├── Logger/                            # (Phase 2) Logger Service
│   ├── include/
│   │   └── dbus_logger.hpp
│   └── src/
│       ├── main.cpp
│       └── dbus_logger.cpp
│
├── config/
│   ├── allowed_ips.conf
│   ├── credentials.conf
│   └── server.conf
│
├── tests/
│   ├── unit/
│   │   ├── test_smpp_message.cpp
│   │   ├── test_smpp_session.cpp
│   │   └── test_ip_validator.cpp
│   ├── integration/
│   │   ├── test_dbus_authenticator.cpp
│   │   └── test_connection_lifecycle.cpp
│   └── e2e/
│       └── quick_test.py
│
├── Docker/
│   └── smsc-dev/
│       └── Dockerfile
│
├── doc/
│   ├── SYSTEM_OOAD_DESIGN.md         # Detailed architecture
│   ├── CURRENT_STATUS.md             # Implementation status
│   ├── BUILD_AND_TEST_PLAN.md        # Build instructions
│   └── SmppServer/                   # SMPP protocol reference
│
└── build/                             # Generated (cmake build output)
```

---

## 🚀 Getting Started

### Prerequisites

- **CMake** 3.12+
- **C++17** compiler (GCC 7+, Clang 5+)
- **ASIO** (async I/O library)
- **spdlog** (logging)
- **D-Bus** (IPC)
- **Docker** (for containerized dev)

### Build

```bash
# Clone and setup
git clone <repo>
cd simple_smpp_server

# Build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run (Docker Development)

```bash
# Start development container
docker build -f Docker/smsc-dev/Dockerfile -t smsc-dev .
docker run -d --name smsc-dev-container \
  -p 2775:2775 \
  -v $(pwd):/workspace \
  smsc-dev

# Start server
docker exec smsc-dev-container /workspace/build/SMPPServer/simple_smpp_server

# Run tests
docker exec smsc-dev-container python tests/quick_test.py
```

### Run (Native - Linux/macOS)

```bash
# Start server
./build/SMPPServer/simple_smpp_server

# In another terminal, run test
python tests/quick_test.py
```

---

## 📊 Configuration

### IP Whitelist

File: `config/allowed_ips.conf`
```
# One IP/CIDR per line
127.0.0.1
::1
192.168.1.0/24
10.0.0.0/8
```

### Credentials

File: `config/credentials.conf`
```
# Format: username:password_hash:salt
test:$2b$12$abc123...:salt123
admin:$2b$12$xyz789...:salt456
```

### Server Configuration

File: `config/server.conf`
```ini
port=2775
log_level=info
max_connections=1000
bind_timeout=5000
```

---

## 🔍 Current Status (Phase 1)

✅ **Implemented**:
- TCP server with ASIO (SMPPServer)
- IP validation (whitelist)
- Logging infrastructure (spdlog)

⚠️ **In Progress**:
- SMPPServer: Client port tracking
- SMPPServer: Process spawner (launch SmppClientHandler with port arg)
- SMPPServer: D-Bus ClaimSocket(port) method
- SmppClientHandler: Parse port from command line args
- SmppClientHandler: D-Bus ClaimSocket caller
- SmppClientHandler: Socket FD receiver

📋 **Next Steps**:
1. Implement SMPPServer process spawner: `SmppClientHandler <port>`
2. Implement SMPPServer D-Bus ClaimSocket method (transfer FD)
3. Implement SmppClientHandler startup with port argument
4. Implement SmppClientHandler D-Bus ClaimSocket caller
5. Implement SMPP PDU parser (SmppClientHandler)
6. Implement BIND/UNBIND/ENQUIRE_LINK handlers
7. Implement D-Bus calls to Authenticator
8. E2E test: Client → SMPPServer → SmppClientHandler → Authenticator

---

## 🗺️ Development Roadmap

### Phase 1: Foundation ✅ (In Progress)
**Goal**: Working SMPP gateway where SMPPServer spawns SmppClientHandler for each client

**SMPPServer**:
- [x] TCP server accepts connections on port 2775
- [x] IP validation (whitelist)
- [ ] Track client ports (ephemeral port numbers)
- [ ] Process spawner: `SmppClientHandler <client_port>`
- [ ] D-Bus service for ClaimSocket(port) method
- [ ] Socket FD transfer via D-Bus

**SmppClientHandler**:
- [ ] Accept port number as command line argument
- [ ] Connect to D-Bus and call ClaimSocket(port)
- [ ] Receive socket FD from SMPPServer
- [ ] SMPP PDU parser
- [ ] BIND handler with Authenticator integration
- [ ] UNBIND and ENQUIRE_LINK handlers
- [ ] Logging via spdlog library
- [ ] E2E test passing (BIND/UNBIND/ENQUIRE_LINK)

### Phase 2: Microservices Refinement (Next)
**Goal**: Implement Authenticator as D-Bus service

- [ ] Implement Authenticator service (D-Bus)
  - D-Bus interface definition
  - Credential store and hashing
  - Integration tests with SmppClientHandler
  
- [ ] Optimize logging infrastructure
  - Log file rotation configuration
  - Audit trail capabilities
  - Per-component log filtering

- [ ] systemd service files
  - simple_smpp_server.service
  - smpp_authenticator.service

### Phase 3: Message Operations
**Goal**: Full SMPP message support

- [ ] SUBMIT_SM (send messages)
- [ ] DELIVER_SM (receive messages)
- [ ] Message queuing
- [ ] Message persistence
- [ ] Retry logic

### Phase 4: Monitoring & HA
**Goal**: Production readiness

- [ ] Prometheus metrics
- [ ] Grafana dashboards
- [ ] Health checks
- [ ] Load balancing
- [ ] Failover support

---

## 🧪 Testing

### Run All Tests

```bash
cd build
ctest --output-on-failure
```

### Unit Tests

```bash
./build/tests/unit/test_smpp_message
./build/tests/unit/test_smpp_session
./build/tests/unit/test_ip_validator
```

### Integration Tests

```bash
./build/tests/integration/test_dbus_authenticator
./build/tests/integration/test_connection_lifecycle
```

### E2E Tests

```bash
python tests/e2e/quick_test.py         # 5-minute smoke test
python tests/e2e/stress_test.py        # 1000 concurrent clients
```

---

## 🏗️ Microservices Communication

### D-Bus IPC

All inter-process communication uses **D-Bus system bus**.

### Socket Transfer via D-Bus

**Key Feature**: SMPPServer provides `ClaimSocket()` D-Bus method. SmppClientHandler calls it to claim the socket.

**Flow**:
1. Client connects to SMPPServer:2775 on ephemeral port (e.g., :54321)
2. SMPPServer accepts connection, validates IP
3. If accepted: SMPPServer spawns `SmppClientHandler 54321`
4. SmppClientHandler starts with port number as argument
5. **SmppClientHandler calls D-Bus method: SMPPServer.ClaimSocket(54321)**
6. **SMPPServer receives the call, finds the socket for port 54321**
7. **SMPPServer transfers socket FD to SmppClientHandler via D-Bus**
8. **SMPPServer closes its end of the socket locally**
9. SmppClientHandler receives FD and takes ownership
10. SmppClientHandler now talks directly to the client

**Benefits**:
- ✅ Process isolation per client (if one crashes, others unaffected)
- ✅ Independent resource management
- ✅ Easy to upgrade SmppClientHandler without restarting
- ✅ Scalable spawning pattern
- ✅ Clean separation: SMPPServer is just an acceptor/dispatcher
- ✅ Each client has its own process space

**D-Bus Interface (Provided by SMPPServer)**:
```xml
<interface name="com.telecom.SMPPServer">
  <method name="ClaimSocket">
    <arg type="u" direction="in" name="client_port"/>
    <arg type="h" direction="out" name="socket_fd"/>
    <arg type="b" direction="out" name="success"/>
  </method>
</interface>
```

**SmppClientHandler Usage**:
```cpp
// SmppClientHandler main.cpp
int main(int argc, char* argv[]) {
    uint16_t client_port = std::stoi(argv[1]);  // e.g., 54321
    
    // Connect to D-Bus
    auto dbus = DBusConnection::system();
    
    // Call SMPPServer.ClaimSocket(port)
    int socket_fd = dbus->call_method(
        "com.telecom.SMPPServer",
        "/com/telecom/SMPPServer",
        "com.telecom.SMPPServer",
        "ClaimSocket",
        client_port
    );
    
    // Now own the socket and handle client
    SmppConnection conn(socket_fd);
    conn.handle_client();
}
```

### Service-to-Service Calls

**SmppClientHandler → Authenticator**:
```cpp
// D-Bus call to validate BIND credentials
bool success = dbus_auth->Authenticate("test", "test");
```

**SmppClientHandler → Logging**:
```cpp
// Direct spdlog usage (no D-Bus)
SPDLOG_INFO("BIND_TRANSMITTER from {}", client_ip);
SPDLOG_WARN("Auth failed for user: {}", username);
SPDLOG_ERROR("PDU parse error: {}", error_msg);
```

### Service Discovery (D-Bus)

Services register on D-Bus:

| Service | Object Path | Interface |
|---------|-------------|-----------|
| **SMPP Server** | `/com/telecom/SMPPServer` | `com.telecom.SMPPServer` |
| **SmppClientHandler** | `/com/telecom/SmppClientHandler` | `com.telecom.SmppClientHandler` |
| **Authenticator** | `/com/telecom/SMPPAuthenticator` | `com.telecom.SMPPAuthenticator` |

---

## 📚 Documentation

### Project Documentation
- **[SYSTEM_OOAD_DESIGN.md](doc/SYSTEM_OOAD_DESIGN.md)** - Complete architecture and design patterns
- **[CURRENT_STATUS.md](doc/CURRENT_STATUS.md)** - Current implementation status and blockers
- **[BUILD_AND_TEST_PLAN.md](doc/BUILD_AND_TEST_PLAN.md)** - Detailed build and test instructions
- **[DBUS_INTERFACES.md](doc/DBUS_INTERFACES.md)** - D-Bus interface specifications and usage examples
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and release notes
- **[VERSIONING.md](doc/VERSIONING.md)** - CMake-based versioning system (config.h.in)
- **[diagrams/](doc/diagrams/)** - PlantUML sequence diagrams (connection flow, authentication, keep-alive)
- **[SmppServer/](doc/SmppServer/)** - SMPP protocol reference and handler documentation

### Component Documentation
- **[SMPP_CLIENT_HANDLER_01_OVERVIEW.md](doc/SMPP_CLIENT_HANDLER_01_OVERVIEW.md)** - SmppClientHandler component overview
- **[SMPP_CLIENT_HANDLER_02_STATE_PATTERN.md](doc/SMPP_CLIENT_HANDLER_02_STATE_PATTERN.md)** - State pattern design
- **[SMPP_CLIENT_HANDLER_03_STATE_TRANSITIONS.md](doc/SMPP_CLIENT_HANDLER_03_STATE_TRANSITIONS.md)** - State machine transitions

### SMPP Protocol Reference
- **[SMPP v5.0 Official Specification](https://smpp.org/SMPP_v5.pdf)** - Complete SMPP protocol definition (external)
- **[SMPP v3.4 Reference](https://en.wikipedia.org/wiki/SMPP)** - Protocol overview and history
- **Protocol Features Implemented**:
  - BIND operations (TRANSMITTER, RECEIVER, TRANSCEIVER)
  - UNBIND graceful disconnection
  - ENQUIRE_LINK keep-alive
  - *(Phase 2): SUBMIT_SM, DELIVER_SM for message operations*

### D-Bus Interface Files

**SMPPServer Service** - Located in `SMPPServer/files/dbus/`:
- `com.opensmsc.smppserver.xml` - SMPPServer D-Bus interface definition
- `com.opensmsc.smppserver.service` - D-Bus service activation file (lowercase)

**Authenticator Service** - Located in `Authenticator/files/dbus/`:
- `com.opensmsc.smppauth.xml` - SMPPAuthenticator D-Bus interface definition
- `com.opensmsc.smppauth.service` - D-Bus service activation file (lowercase)

---

## 🤝 Contributing

### Code Style
- C++17 standards
- Follow existing patterns in codebase
- Use ASIO for async operations
- Use spdlog for logging
- D-Bus for IPC

### Before Committing
1. Run all tests: `ctest`
2. Check code compiles: `cmake --build .`
3. Verify no warnings: Clean build with `-Werror`

---

## 📝 License

[Your License Here]

---

## 📧 Contact

**Owner**: Development Team  
**Email**: sahoorajesh.d@gmail.com  
**Last Updated**: 2026-04-20

---

## 🎓 Key Design Principles

1. **Single Responsibility** - Each service has one job
2. **Loose Coupling** - Services communicate via D-Bus, not direct calls
3. **High Cohesion** - Related functionality grouped together
4. **Testability** - Components designed for unit testing
5. **Observability** - Centralized logging for all events
6. **Fail-Safe** - Graceful degradation when services unavailable
7. **Scalability** - Async I/O, non-blocking, connection pooling

---

## ⚡ Advantages of Process-Per-Client Architecture

| Advantage | Benefit |
|-----------|---------|
| **Process Isolation** | One client crash doesn't affect others; each has own memory space |
| **Independent Scaling** | Easy to add/remove clients; no global connection limit bottleneck |
| **Resource Control** | Each handler process can have its own limits (CPU, memory, file handles) |
| **Graceful Upgrades** | Can upgrade SmppClientHandler binary without killing active connections |
| **Fault Resilience** | Failed client handler automatically cleaned up; parent continues accepting new clients |
| **Simple State Management** | Per-process state = no shared mutable state complexity |
| **Security Boundary** | Each client interaction isolated from others; easier to audit/monitor |
| **Debugging** | Can attach debugger to individual client process without affecting others |

---

## 🎓 Key Design Principles

1. **Single Responsibility** - Each process has one job:
   - SMPPServer: Accept connections + IP validate
   - SmppClientHandler: Protocol handling for one client
   - Authenticator: Authentication service
   - spdlog: Shared logging library (no service overhead)

2. **Process Isolation** - Failures are contained:
   - One bad client handler doesn't crash SMPPServer
   - One service unavailability doesn't crash others

3. **Loose Coupling** - Services talk via D-Bus, not direct calls

4. **High Cohesion** - Related functionality grouped together

5. **Testability** - Components designed for unit testing

6. **Observability** - Centralized logging for all events

7. **Fail-Safe** - Graceful degradation when services unavailable

8. **Scalability** - Async I/O + per-process model scales to many concurrent clients

---

**Status**: 🟡 In Development  
**Last Verified**: 2026-04-20
