# SMPP Client Handler - Overview

## Component Description

The SmppClientHandler is a single socket SMPP client that handles one SMPP client connection at a time. It receives a pre-established TCP socket file descriptor from the D-Bus service and manages the complete SMPP session lifecycle.

### Key Characteristics
- **Single Socket Handler**: Manages exactly one SMPP connection
- **File Descriptor Based**: Created from a pre-connected socket FD (not accepting connections)
- **D-Bus Integration**: Receives socket FD from dbus-based socket handoff mechanism
- **Stateful Session Management**: Maintains SMPP protocol state throughout the connection
- **Protocol Compliant**: Implements SMPP 3.4 specification

## Architecture

### High-Level Components

```
┌─────────────────────────────────────────┐
│  D-Bus Service                          │
│  (Provides connected socket FD)          │
└──────────────┬──────────────────────────┘
               │
        (socket handoff)
               │
               ▼
┌──────────────────────────────────────────┐
│  SmppClientHandler                       │
│  ┌────────────────────────────────────┐ │
│  │ SmppStateMachine (State Pattern)    │ │
│  │ - IDLE, CONNECTED, BINDING         │ │
│  │ - BOUND_TX, BOUND_RX, BOUND_TRX    │ │
│  │ - UNBINDING, DISCONNECTED, ERROR   │ │
│  └────────────────────────────────────┘ │
│  ┌────────────────────────────────────┐ │
│  │ SMPPFramer (PDU Extraction)         │ │
│  │ - Extract PDUs from socket stream  │ │
│  │ - Route to state machine           │ │
│  └────────────────────────────────────┘ │
│  ┌────────────────────────────────────┐ │
│  │ Session Manager                    │ │
│  │ - Track binding status             │ │
│  │ - Manage timers                    │ │
│  │ - Connection metadata              │ │
│  └────────────────────────────────────┘ │
└──────────────────────────────────────────┘
           │
    (TCP socket)
           │
           ▼
┌──────────────────────────────────────────┐
│  SMPP Server                             │
└──────────────────────────────────────────┘
```

## Protocol Support

### Phase 1 - Core BIND/UNBIND Operations

**BIND PDUs (3 Types)**
- **BIND_TRANSMITTER** (0x00000002): TX-only connection
- **BIND_RECEIVER** (0x00000001): RX-only connection
- **BIND_TRANSCEIVER** (0x00000009): Bidirectional connection

**Session Management**
- **UNBIND** (0x00000006): Graceful disconnect
- **ENQUIRE_LINK** (0x00000015): Keep-alive

### Phase 2 - SMS Operations (Future)
- SUBMIT_SM / SUBMIT_SM_RESP (Send SMS)
- DELIVER_SM / DELIVER_SM_RESP (Receive SMS)

## Dependencies

- **sdbus-c++**: D-Bus socket handoff
- **spdlog**: Logging framework
- **asio** (standalone): Async I/O
- **smppcxx**: SMPP protocol library

## Key Features

- **State Pattern Design**: Clean separation of state logic
- **Type-Safe PDU Handling**: Compiler checks invalid PDU operations
- **Exception-Based Error Handling**: SmppStateException for protocol violations
- **Callback Architecture**: State transitions, bind success, error handling
- **smppcxx Integration**: Uses standard SMPP types and constants
