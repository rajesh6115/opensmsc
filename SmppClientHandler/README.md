# SMPP Client Handler

A single-socket SMPP client component that manages one SMPP protocol session. Receives a pre-established socket file descriptor from D-Bus and handles complete session lifecycle including authentication (BIND), keep-alive (ENQUIRE_LINK), and graceful shutdown (UNBIND).

## Quick Overview

- **Purpose**: Handle single SMPP client connection
- **Socket Source**: Pre-established FD from D-Bus service
- **Session States**: IDLE → CONNECTED → BINDING → BOUND_TX/RX/TRX → UNBINDING → DISCONNECTED
- **Phase 1 PDUs**: BIND (3 types), UNBIND, ENQUIRE_LINK
- **Architecture**: State Pattern with exception-based error handling

## Key Dependencies

- `sdbus-c++` - D-Bus socket handoff
- `spdlog` - Structured logging
- `asio` (standalone) - Async TCP I/O
- `smppcxx` - SMPP protocol types

## Directory Structure

```
SmppClientHandler/
├── include/
│   ├── smpp_client_handler.hpp      # Main client handler
│   ├── smpp_state_machine_v2.hpp    # State machine orchestrator
│   ├── smpp_state_base.hpp          # Base state class
│   └── smpp_states.hpp              # Concrete state implementations
├── src/
│   ├── main.cpp                     # Entry point
│   ├── smpp_client_handler.cpp      # Implementation
│   ├── smpp_state_machine_v2.cpp    # State machine implementation
│   └── smpp_states.cpp              # State implementations
└── CMakeLists.txt                   # Build configuration
```

## Building

```bash
cd SmppClientHandler
mkdir build && cd build
cmake ..
make
```

## Documentation

Complete documentation is in the global `doc/` folder:

1. **[SMPP_CLIENT_HANDLER_01_OVERVIEW.md](../doc/SMPP_CLIENT_HANDLER_01_OVERVIEW.md)** - Architecture and dependencies
2. **[SMPP_CLIENT_HANDLER_02_STATE_PATTERN.md](../doc/SMPP_CLIENT_HANDLER_02_STATE_PATTERN.md)** - Design pattern details
3. **[SMPP_CLIENT_HANDLER_03_STATE_TRANSITIONS.md](../doc/SMPP_CLIENT_HANDLER_03_STATE_TRANSITIONS.md)** - State machine transitions

## Implementation Status

- ✅ State machine design (State Pattern)
- ✅ BIND/UNBIND/ENQUIRE_LINK support
- ⏳ PDU framing (SMPPFramer)
- ⏳ Session metadata management
- ⏳ Timer/timeout handling

## Key Features

- **Type-Safe State Handling**: Each state implements only valid PDU handlers
- **Exception-Based Errors**: Invalid PDUs throw `SmppStateException`
- **Callback Architecture**: State transitions, bind success, unbind success, errors
- **smppcxx Integration**: Uses standard SMPP types and constants
- **Extensible Design**: Phase 2 SMS features can be added without core changes
