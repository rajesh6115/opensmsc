# Phase 1.1 - SMPP Protocol Implementation Plan

**Phase**: 1.1 - Core Protocol Operations (MVP)  
**Status**: Starting Implementation  
**Date**: 2026-04-19  
**Duration**: Estimated 3-5 days  

---

## 🎯 Phase 1.1 Objectives

### Primary Goals
1. **Fix Compilation Errors** - Make the project compile cleanly
2. **Implement SMPP Operations** - BIND, UNBIND, ENQUIRE_LINK
3. **Code Quality** - Remove globals, improve architecture
4. **Testing** - Unit tests and manual verification

### Success Criteria
- ✅ Project compiles without errors
- ✅ SMPP server accepts client connections
- ✅ BIND operations (all variants) working
- ✅ UNBIND graceful disconnect working
- ✅ ENQUIRE_LINK keep-alive working
- ✅ Unit tests passing
- ✅ Can test with SMPP clients (e.g., smppp)

---

## 📋 Implementation Steps

### Step 1.1.1: Fix Compilation Errors (CRITICAL)
**Effort**: 2-4 hours  
**Blocking**: YES

#### Sub-tasks
- [ ] **1.1.1.1** Fix `smpp_service_manager.cpp` sdbus-c++ API
  - Remove incorrect `ServiceName` usage
  - Fix `createProxy()` initialization
  - Research correct sdbus-c++ 1.4.0 API
  - Options:
    - Option A: Remove D-Bus integration for now (simplified)
    - Option B: Fix D-Bus proxy creation (proper)
  - **Recommendation**: Option A for MVP (D-Bus monitoring is Phase 2+)

- [ ] **1.1.1.2** Verify clean compilation
  ```bash
  cd /workspace/build
  cmake --build . 2>&1 | grep -E "error|warning"
  ```

- [ ] **1.1.1.3** Verify executable creation
  ```bash
  ls -lh /workspace/build/simple_smpp_server
  ```

### Step 1.1.2: Code Cleanup - Remove Global Variables
**Effort**: 1-2 hours

#### Sub-tasks
- [ ] **1.1.2.1** Replace global `g_io_ctx` with `asio::signal_set`
  - In `main.cpp`:
    - Remove: `static asio::io_context* g_io_ctx = nullptr`
    - Remove: `on_signal()` free function
    - Add: `asio::signal_set signals(io_ctx, SIGINT, SIGTERM)`
    - Add: `signals.async_wait([&io_ctx](...) { io_ctx.stop(); })`

- [ ] **1.1.2.2** Test signal handling
  - Start server
  - Press Ctrl+C
  - Verify graceful shutdown

### Step 1.1.3: Understand SMPP Protocol Basics
**Effort**: 2-3 hours (research + design)

#### Sub-tasks
- [ ] **1.1.3.1** Study SMPP v3.4 packet structure
  - Header format (16 bytes)
  - PDU body format
  - Status codes and error handling

- [ ] **1.1.3.2** Design SMPP message handling
  - Packet parsing from TCP stream
  - Response generation
  - State machine for client lifecycle

- [ ] **1.1.3.3** Plan data structures
  - SMPP PDU classes
  - Client session state
  - Command handlers

### Step 1.1.4: Implement SMPP BIND Operation
**Effort**: 4-6 hours (implementation + testing)

#### Architecture
```
TCP Client Connection
    ↓
TcpServer::on_accept()
    ↓
SmppServiceManager::handle_bind()
    ↓
Parse BIND_TRANSMITTER/RECEIVER/TRANSCEIVER
    ↓
Validate credentials
    ↓
Send BIND_*_RESP
    ↓
Track client session
```

#### Sub-tasks
- [ ] **1.1.4.1** Design SMPP PDU classes
  - BindRequest: parse from wire
  - BindResponse: serialize to wire
  - Use smppcxx library for encoding/decoding

- [ ] **1.1.4.2** Implement bind request handling
  - In `SmppServiceManager`: add `handle_bind()`
  - Parse command type from SMPP header
  - Validate username/password
  - Generate response PDU

- [ ] **1.1.4.3** Integrate with TcpServer
  - Pass incoming PDU to service manager
  - Send response back to client
  - Track authenticated sessions

- [ ] **1.1.4.4** Manual testing
  - Use SMPP test client (e.g., `smppp`)
  - Test BIND_TRANSMITTER
  - Test BIND_RECEIVER
  - Test BIND_TRANSCEIVER
  - Verify responses

### Step 1.1.5: Implement SMPP UNBIND Operation
**Effort**: 2-3 hours

#### Sub-tasks
- [ ] **1.1.5.1** Implement unbind handler
  - In `SmppServiceManager`: add `handle_unbind()`
  - Send UNBIND_RESP
  - Close client connection

- [ ] **1.1.5.2** Test unbind
  - Connect client
  - Bind
  - Send UNBIND
  - Verify graceful disconnect

### Step 1.1.6: Implement SMPP ENQUIRE_LINK (Keep-Alive)
**Effort**: 2-3 hours

#### Sub-tasks
- [ ] **1.1.6.1** Implement enquire_link handler
  - In `SmppServiceManager`: add `handle_enquire_link()`
  - Send ENQUIRE_LINK_RESP with same sequence number

- [ ] **1.1.6.2** Test keep-alive
  - Connect and bind
  - Send ENQUIRE_LINK periodically
  - Verify responses

### Step 1.1.7: Code Quality & Testing
**Effort**: 3-4 hours

#### Sub-tasks
- [ ] **1.1.7.1** Add unit tests
  - Test SMPP PDU parsing
  - Test credential validation
  - Test response generation

- [ ] **1.1.7.2** Code review
  - Remove any remaining globals
  - Check error handling
  - Verify thread safety

- [ ] **1.1.7.3** Integration testing
  - Start server
  - Run multiple clients simultaneously
  - Verify concurrent handling

### Step 1.1.8: Documentation & Commit
**Effort**: 1-2 hours

#### Sub-tasks
- [ ] **1.1.8.1** Update code comments
  - Document SMPP operation handlers
  - Document message flow

- [ ] **1.1.8.2** Create execution report
  - Record implementation decisions
  - Document API usage patterns
  - Note any deviations from plan

- [ ] **1.1.8.3** Commit to git
  - Create single clean commit or multiple logical commits
  - Push to `main` branch

---

## 🛠️ Implementation Details

### SMPP Operations Summary

| Operation | Direction | Purpose | Status |
|-----------|-----------|---------|--------|
| **BIND_TRANSMITTER** | Client → Server | Authenticate as transmitter | TODO |
| **BIND_RECEIVER** | Client → Server | Authenticate as receiver | TODO |
| **BIND_TRANSCEIVER** | Client → Server | Authenticate as both | TODO |
| **BIND_*_RESP** | Server → Client | Bind success/failure | TODO |
| **UNBIND** | Client → Server | Graceful disconnect | TODO |
| **UNBIND_RESP** | Server → Client | Acknowledge unbind | TODO |
| **ENQUIRE_LINK** | Both | Keep-alive heartbeat | TODO |
| **ENQUIRE_LINK_RESP** | Both | Keep-alive response | TODO |

### Key Files to Modify

```
SMPPServer/
├── src/
│   ├── main.cpp                      ← Remove global g_io_ctx
│   ├── tcp_server.cpp                ← Handle PDU dispatch
│   ├── smpp_service_manager.cpp      ← Fix API + implement handlers
│   └── (NEW) smpp_protocol.cpp       ← SMPP PDU handling
├── include/
│   ├── tcp_server.hpp
│   ├── smpp_service_manager.hpp
│   └── (NEW) smpp_protocol.hpp       ← SMPP PDU classes
└── tests/
    └── (NEW) test_smpp_operations.cpp ← Unit tests
```

### Testing Tools

```bash
# Test SMPP server
# Option 1: Use SMPP client library (e.g., smppp)
apt-get install smppclient

# Option 2: Use netcat + manual PDU construction
nc localhost 2775

# Option 3: Use Python smpplib
pip install smpplib
```

---

## 🎯 Daily Checkpoints

### Day 1 (2-3 hours)
- [ ] Compile errors fixed
- [ ] Global variables removed
- [ ] Basic SMPP protocol understood

### Day 2 (4-6 hours)
- [ ] BIND operations implemented
- [ ] Manual testing with SMPP client
- [ ] Graceful error handling in place

### Day 3 (2-4 hours)
- [ ] UNBIND working
- [ ] ENQUIRE_LINK working
- [ ] Unit tests passing

### Day 4 (2-3 hours)
- [ ] Integration testing complete
- [ ] Code review & cleanup
- [ ] Documentation done

### Day 5 (1-2 hours)
- [ ] Final testing
- [ ] Commit & push
- [ ] Prepare for Phase 1.2

---

## 📊 Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|-----------|
| SMPP library API confusion | HIGH | Read smppcxx docs, test incrementally |
| TCP stream parsing issues | HIGH | Test with known SMPP clients |
| Thread safety problems | MEDIUM | Use ASIO's thread pool correctly |
| Credential validation | LOW | Simple username/password match |

---

## 🚀 Success Metrics

### Functionality
- ✅ Server compiles cleanly (0 errors, minimal warnings)
- ✅ Server starts without crashing
- ✅ Accepts SMPP client connections
- ✅ Handles BIND/UNBIND/ENQUIRE_LINK correctly
- ✅ Responds with valid SMPP PDUs

### Code Quality
- ✅ No global variables (except static constants)
- ✅ Proper error handling
- ✅ Thread-safe operations
- ✅ Unit test coverage

### Testing
- ✅ Manual testing with SMPP clients
- ✅ Unit tests passing
- ✅ No memory leaks (verified with valgrind if available)

---

## 📝 Phase 1.1 Completion Criteria

Phase 1.1 is **COMPLETE** when:

1. ✅ Project compiles without errors
2. ✅ SMPP server listens on port 2775
3. ✅ BIND operations (all three variants) working
4. ✅ UNBIND operation working
5. ✅ ENQUIRE_LINK keep-alive working
6. ✅ Unit tests passing
7. ✅ Code review completed
8. ✅ Execution report documented
9. ✅ Committed to git main branch
10. ✅ Ready for Phase 1.2 (message operations)

---

## 📚 References

### SMPP Protocol
- [SMPP v3.4 Specification](https://en.wikipedia.org/wiki/Short_Message_Peer-to-Peer)
- Command codes and PDU formats
- Error codes and status values

### ASIO Documentation
- [ASIO Signal Set](https://think-async.com/Asio/asio-1.24.0/doc/asio/reference/signal_set.html)
- [ASIO TCP](https://think-async.com/Asio/)

### smppcxx Library
- [GitHub: smppcxx](https://github.com/rajesh6115/smppcxx)
- PDU encoding/decoding examples

---

**Next**: Begin Step 1.1.1 - Fix Compilation Errors

Ready to start? Confirm and we'll begin Phase 1.1 implementation! 🚀
