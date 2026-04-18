# SMSC (Simple SMPP Server) - Project Plan

**Version**: 1.0  
**Last Updated**: 2026-04-18  
**Status**: Planning Phase  
**Team**: Solo Developer  
**Timeline**: No deadline (Flexible)

---

## 1. Project Vision & Purpose

Build a **production-grade SMPP (Short Message Service Protocol) server** that can:
- Accept and authenticate SMPP client connections
- Serve as a telecom gateway for SMS message routing
- Provide a foundation for future enhancements (message routing, storage, monitoring)

**Target**: Start with MVP (Minimum Viable Product) - focus on reliability and correctness, not scale initially.

---

## 2. High-Level Goals

### Phase 1: MVP - SMPP Bind Support (Foundation)
- **Goal**: Build a working SMPP server that can authenticate clients
- **Success Criteria**:
  - Server accepts SMPP TCP connections
  - Handles BIND_TRANSMITTER, BIND_RECEIVER, BIND_TRANSCEIVER operations
  - Properly validates bind credentials
  - Responds with appropriate SMPP bind responses (success/failure)
  - Basic logging and error handling

### Phase 2: Message Operations (Future)
- Support SUBMIT_SM (client → server)
- Support DELIVER_SM (server → client)
- Basic message queueing

### Phase 3: Persistence & Monitoring (Future)
- Database storage for messages/logs
- Admin interface or monitoring tools
- Performance optimization

---

## 3. Technology Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| **Language** | C++17+ | Performance, system-level control |
| **Build System** | CMake 3.20+ | Modern, cross-platform, already started |
| **SMPP Library** | smppcxx (github.com/rajesh6115/smppcxx) | SMPP PDU encoding/decoding, SMPP v5.0 support |
| **Concurrency** | C++11 std::thread (MVP) | Simple initially, optimize later |
| **Containerization** | Docker (Ubuntu 24.04) | Reproducible builds, deployment |
| **Testing** | Catch2 or Google Test | C++ unit testing |

---

## 4. Project Phases & Milestones

```
Phase 1: MVP - Bind Support
├── Phase 1.1: Setup & Integration
│   ├── Integrate smppcxx library (in external/)
│   ├── Set up CMakeLists.txt to build with smppcxx
│   └── Create project skeleton with server structure
├── Phase 1.2: Core Server Implementation
│   ├── TCP server listening
│   ├── SMPP packet parsing (using library)
│   └── Bind request handling
├── Phase 1.3: Testing & Polish
│   ├── Unit tests
│   ├── Integration tests with SMPP clients
│   └── Error handling & edge cases
└── [REVIEW & RELEASE MVP]

Phase 2: Message Operations (planned, not started)
Phase 3: Persistence & Monitoring (planned, not started)
```

---

## 5. Definition of "Done" for Phase 1

A step is complete when:
1. ✅ Code is written and compiles without warnings
2. ✅ Tests pass (unit + integration)
3. ✅ Documented in `doc/projectexecution/<step_name>.md`
4. ✅ Committed to git with clear message
5. ✅ Integrated into main build (CMakeLists.txt updated)

---

## 6. Development Process (Agile Approach)

### Iteration Cycle
1. **Plan** the step (update plan if needed)
2. **Execute** the step (implement + test)
3. **Document** (write execution notes in `doc/projectexecution/<step_name>.md`)
4. **Review** (commit, verify)
5. **Iterate** (move to next step)

### Documentation Requirements
Each execution step should document:
- What was accomplished
- Key decisions made
- Any challenges/blockers encountered
- How to test/verify the work
- Next steps

### Plan Updates
- Review and update `doc/projectplan.md` if:
  - Scope changes
  - New insights alter the approach
  - Milestones need adjustment
  - New blockers discovered

---

## 7. Known Assumptions & Constraints

- Solo developer → prioritize clarity and documentation
- No deadline → quality over speed
- MVP focus → minimal scope, then expand
- Use existing SMPP library → avoid reinventing the wheel
- Docker available for consistent builds

---

## 8. Appendix: SMPP Protocol Basics (Quick Reference)

- **SMPP**: Simple Message Peer-to-Peer Protocol
- **Versions**: We'll target SMPP v3.4 (most common)
- **Core Operations for MVP**:
  - `BIND_TRANSMITTER` (0x00000001) - client sends messages only
  - `BIND_RECEIVER` (0x00000004) - client receives messages only
  - `BIND_TRANSCEIVER` (0x00000009) - bidirectional
  - Response: `BIND_TRANSMITTER_RESP`, `BIND_RECEIVER_RESP`, `BIND_TRANSCEIVER_RESP`

---

## Next Step
→ **Phase 1.1: Setup & Integration - Integrate smppcxx library into build**

Start: 2026-04-18

---

## Library Integration Notes

**Imported**: smppcxx (https://github.com/rajesh6115/smppcxx)  
**Location**: `external/smppcxx/` (as git submodule)  
**Features**: SMPP PDU encoding/decoding, SMPP v5.0 support  
**Usage**: Header files in `external/smppcxx/include/`, can be integrated into CMakeLists.txt  
**Maintenance**: Can be modified/updated in both this project and the original repo
