# SMPPServer Low Level Design Documentation

**Date**: 2026-04-20  
**Version**: 2.0  
**Status**: Updated for Process-Per-Client Microservices Architecture  
**Architecture**: Microservices with separate SMPPServer and SmppClientHandler processes

---

## Architecture Overview

This project uses a **process-per-client microservices model**:

- **SMPPServer** (single process)
  - Listens on TCP port 2775
  - Accepts client connections
  - Validates client IPs
  - Spawns `SmppClientHandler <port>` for each valid client
  - Provides D-Bus `ClaimSocket(port)` method for socket handoff

- **SmppClientHandler** (per-client process)
  - Started by SMPPServer
  - Claims socket from SMPPServer via D-Bus
  - Handles SMPP protocol (PDU parsing, authentication, etc.)
  - Calls Authenticator service (D-Bus)
  - Logs via spdlog library

- **Authenticator** (separate D-Bus service)
  - Validates SMPP credentials
  - Returns authentication result to SmppClientHandler

See [../SYSTEM_OOAD_DESIGN.md](../SYSTEM_OOAD_DESIGN.md) for full system architecture.

---

## Documentation Structure

This directory contains detailed Low Level Design (LLD) documentation for implementing the SMPP Server and SmppClientHandler components in the microservices architecture.

**📋 START HERE**: Read [ARCHITECTURE_UPDATE_v2.md](ARCHITECTURE_UPDATE_v2.md) for the complete microservices design (v2.0)

```
doc/SmppServer/
├── README.md (this file) 
├── ARCHITECTURE_UPDATE_v2.md              [⭐ START HERE: v2.0 design]
├── LLD_Overview.md                        [High-level LLD summary]
│
├── class_specifications/
│   ├── presentation_layer.md                [TcpServer, SmppConnection, SmppServer]
│   ├── protocol_layer.md                    [SmppMessage, Parser, Encoder]
│   ├── business_logic_layer.md              [SmppSession, Processor, Handlers]
│   └── infrastructure_layer.md              [Validators, Authenticator, Logger, Manager]
│
├── implementation_guides/
│   ├── class_implementation_checklist.md    [Step-by-step implementation order]
│   ├── header_file_templates.md             [Template structure for .hpp files]
│   ├── cpp_file_templates.md                [Template structure for .cpp files]
│   └── common_patterns.md                   [Coding patterns to follow]
│
├── testing/
│   ├── unit_testing_strategy.md             [How to test each class]
│   ├── integration_testing_strategy.md      [How to test class interactions]
│   ├── test_cases.md                        [Specific test cases per class]
│   └── mock_objects.md                      [Mock/stub definitions]
│
├── build_system/
│   ├── cmake_configuration.md               [CMakeLists.txt updates needed]
│   └── dependency_management.md             [Third-party library integration]
│
└── error_handling/
    ├── error_codes.md                       [SMPP status codes reference]
    ├── exception_handling.md                [Exception strategies per class]
    └── recovery_strategies.md               [How to handle failures]
```

---

## How to Use This Documentation

### For Implementing a New Class:

1. **Read**: `LLD_Overview.md` - Understand the context
2. **Find**: The appropriate layer doc in `class_specifications/` 
3. **Study**: The specific class specification
4. **Review**: `implementation_guides/class_implementation_checklist.md` - see the order
5. **Use**: Templates from `implementation_guides/header_file_templates.md`
6. **Code**: Implement following patterns in `implementation_guides/common_patterns.md`
7. **Test**: Follow `testing/test_cases.md` for that class
8. **Integrate**: See `testing/integration_testing_strategy.md`

### For Understanding a Specific Topic:

- **Class Details**: → `class_specifications/` (by layer)
- **How to Test**: → `testing/`
- **How to Build**: → `build_system/`
- **Error Handling**: → `error_handling/`
- **Coding Standards**: → `implementation_guides/`

---

## Related High-Level Documents

These documents provide context for the LLD:

| Document | Purpose | Location |
|---|---|---|
| SYSTEM_OOAD_DESIGN.md | System-wide architecture | `doc/` |
| SMPPServer_High_Level_Design.md | Component design (layers, patterns) | `doc/` |
| CLASS_DESIGN_PLAN.md | All classes with responsibilities | `doc/` |
| SEQUENCE_DIAGRAMS.md | How classes interact (flows) | `doc/` |
| BUILD_AND_TEST_PLAN.md | Integration testing procedures | `doc/` |

---

## Quick Navigation

### By Layer

**Presentation Layer** (Network I/O) - **SMPPServer Only**
- Document: `class_specifications/presentation_layer.md`
- Classes: 
  - `SmppServer` - Main server process, accepts connections
  - `TcpServer` - Low-level TCP socket management
  - `IpValidator` - IP whitelist validation
  - `ProcessSpawner` - Spawns SmppClientHandler with port

**Protocol Layer** (SMPP Protocol) - **SmppClientHandler**
- Document: `class_specifications/protocol_layer.md`
- Classes: 
  - `SmppMessage` - SMPP message structure
  - `SmppMessageParser` - Parse PDU bytes into messages
  - `SmppMessageEncoder` - Encode messages into PDU bytes

**Business Logic Layer** (Application Logic) - **SmppClientHandler**
- Document: `class_specifications/business_logic_layer.md`
- Classes: 
  - `SmppSession` - Per-client session state machine
  - `SmppMessageProcessor` - Route messages to handlers
  - `BindHandler` - Handle BIND requests
  - `UnbindHandler` - Handle UNBIND requests
  - `EnquireLinkHandler` - Handle ENQUIRE_LINK requests

**Infrastructure Layer** (D-Bus & Utilities) - **Shared**
- Document: `class_specifications/infrastructure_layer.md`
- Classes: 
  - `DBusAuthenticator` - Call Authenticator service (SmppClientHandler)
  - `SocketTransfer` - D-Bus socket handoff (SMPPServer side)

---

### By Implementation Phase

**Phase 1.A: SMPPServer Executable** (Start here - simple!)
1. TcpServer (presentation_layer.md) - Accept connections
2. IpValidator (presentation_layer.md) - Validate IPs
3. ProcessSpawner (presentation_layer.md) - Spawn handlers
4. SocketTransfer (infrastructure_layer.md) - D-Bus socket handoff
5. SmppServer (presentation_layer.md) - Main orchestrator

**Phase 1.B: SmppClientHandler Executable** (After SMPPServer)
1. SmppMessage (protocol_layer.md) - Message structure
2. SmppSession (business_logic_layer.md) - State machine
3. SmppMessageParser (protocol_layer.md) - PDU parsing
4. SmppMessageProcessor (business_logic_layer.md) - Message routing
5. Handlers (business_logic_layer.md):
   - BindHandler (BIND_TRANSMITTER/RECEIVER/TRANSCEIVER)
   - UnbindHandler
   - EnquireLinkHandler
6. DBusAuthenticator (infrastructure_layer.md) - Call Authenticator service

**Phase 1.C: Infrastructure** (Support both)
- Logging via spdlog (library, not service)
- D-Bus interfaces (com.opensmsc.smppserver, com.opensmsc.smppauth)

---

## Implementation Workflow

### For SMPPServer Executable
```
1. Review ../SYSTEM_OOAD_DESIGN.md (system context)
2. Read presentation_layer.md (TcpServer, SmppServer)
3. Implement in order:
   - TcpServer (raw socket I/O)
   - IpValidator (IP whitelist check)
   - ProcessSpawner (exec SmppClientHandler)
   - SocketTransfer (D-Bus socket handoff)
   - SmppServer (orchestrator)
4. Write unit tests for each class
5. Integration test: Client connects → SMPPServer accepts → Spawns handler
```

### For SmppClientHandler Executable
```
1. Review ../SYSTEM_OOAD_DESIGN.md (system context)
2. Read protocol_layer.md and business_logic_layer.md
3. Implement foundation classes first:
   - SmppMessage (protocol structure)
   - SmppSession (state machine)
   - SmppMessageParser (PDU → Message)
4. Implement SmppMessageProcessor (message router)
5. Implement handlers:
   - BindHandler (auth via D-Bus)
   - UnbindHandler
   - EnquireLinkHandler
6. Write unit tests for each class
7. Integration test: BIND request → parse → auth → response
```

### For Both
```
- See templates in implementation_guides/
- Follow patterns in common_patterns.md
- Handle errors per error_handling/
- See SEQUENCE_DIAGRAMS.md for flow
- Test per testing/test_cases.md
```

---

## Document Status

| Document | Status | Purpose |
|---|---|---|
| README.md | ✓ Complete | This file - navigation guide |
| LLD_Overview.md | ⏳ Pending | High-level LLD summary |
| presentation_layer.md | ⏳ Pending | TcpServer, SmppConnection, SmppServer specs |
| protocol_layer.md | ⏳ Pending | SmppMessage, Parser, Encoder specs |
| business_logic_layer.md | ⏳ Pending | SmppSession, Processor, Handlers specs |
| infrastructure_layer.md | ⏳ Pending | Validators, Authenticator, Logger specs |
| class_implementation_checklist.md | ⏳ Pending | Implementation order and checklist |
| header_file_templates.md | ⏳ Pending | .hpp template structure |
| cpp_file_templates.md | ⏳ Pending | .cpp template structure |
| common_patterns.md | ⏳ Pending | Coding patterns and standards |
| unit_testing_strategy.md | ⏳ Pending | Unit testing approach |
| integration_testing_strategy.md | ⏳ Pending | Integration testing approach |
| test_cases.md | ⏳ Pending | Specific test cases per class |
| mock_objects.md | ⏳ Pending | Mock/stub object definitions |
| cmake_configuration.md | ⏳ Pending | CMakeLists.txt updates |
| dependency_management.md | ⏳ Pending | Third-party library integration |
| error_codes.md | ⏳ Pending | SMPP status codes reference |
| exception_handling.md | ⏳ Pending | Exception strategies |
| recovery_strategies.md | ⏳ Pending | Failure recovery approaches |

---

## Key Architectural Changes (v2.0)

### What Changed from v1.0?

**v1.0 (Monolithic)**: Single SMPPServer process handles connections AND protocol logic
- SmppServer spawned threads for each client
- All protocol processing in-process
- Shared state management

**v2.0 (Microservices)**: Process-per-client model with D-Bus IPC
- SMPPServer: ONLY accept connections + dispatch
- SmppClientHandler: NEW separate process per client
- Socket handoff via D-Bus (not shared FD)
- Authenticator: Separate D-Bus service
- Logging: spdlog library (not a service)

### Benefits of v2.0
- ✅ Process isolation: client crash doesn't affect others
- ✅ Simple SMPPServer: less code, fewer bugs
- ✅ Resource control: limits per handler process
- ✅ Graceful upgrade: replace handler without restart
- ✅ Better testability: each process independently tested

### Implementation Impact
- **Simpler SMPPServer code**: Focus on TCP I/O only
- **More handlers needed**: SmppClientHandler implementation is larger
- **D-Bus required**: System infrastructure dependency
- **Socket transfer complexity**: Need D-Bus FD passing

---

## Key Design Principles (for reference)

1. **Separation of Concerns**: Each class has one responsibility
2. **Dependency Injection**: Dependencies passed in, not created
3. **Immutable Value Objects**: SmppMessage, SmppSession state queries
4. **State Machine**: SmppSession controls valid transitions
5. **Async I/O**: Non-blocking socket operations via ASIO
6. **Error Handling**: Clear exception paths, no silent failures
7. **Logging**: All important decisions logged at appropriate levels
8. **Testability**: All classes independently testable with mocks

---

## Quick Reference: Class Dependencies

### SMPPServer Process
```
SmppServer (main.cpp)
  ├─ TcpServer
  │   └─ IpValidator
  │       └─ allowed_ips.conf (file)
  ├─ ProcessSpawner
  │   └─ SmppClientHandler binary
  └─ SocketTransfer
      └─ D-Bus /com/opensmsc/SMPPServer
```

### SmppClientHandler Process (per client)
```
SmppClientHandler (main.cpp)
  ├─ SocketReceiver
  │   └─ D-Bus ClaimSocket(port)
  ├─ SmppConnection
  │   ├─ SmppSession
  │   ├─ SmppMessageParser
  │   ├─ SmppMessageProcessor
  │   │   ├─ BindHandler
  │   │   ├─ UnbindHandler
  │   │   ├─ EnquireLinkHandler
  │   │   └─ handlers (more in Phase 2)
  │   └─ DBusAuthenticator
  │       └─ D-Bus /com/opensmsc/SMPPAuthenticator
  └─ Logger (spdlog library)
```

---

## Contact & Questions

For questions about:
- **Overall architecture**: See SYSTEM_OOAD_DESIGN.md
- **Class design**: See CLASS_DESIGN_PLAN.md or relevant class_specifications/ doc
- **Implementation order**: See class_implementation_checklist.md
- **Testing approach**: See testing/ directory
- **Error handling**: See error_handling/ directory

---

**Last Updated**: 2026-04-20  
**Next Step**: Update class_specifications/ to reflect new architecture  
**Owner**: Development Team

---

## Related Documentation

- **[README.md](../../README.md)** - Project overview and architecture
- **[SYSTEM_OOAD_DESIGN.md](../SYSTEM_OOAD_DESIGN.md)** - System-wide architecture
- **[DBUS_INTERFACES.md](../DBUS_INTERFACES.md)** - D-Bus interface specifications
- **[diagrams/](../diagrams/)** - PlantUML sequence diagrams
