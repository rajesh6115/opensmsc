# Build and Integration Testing Plan

**Date**: 2026-04-19  
**Target**: SMPP Server Phase 1.1 Complete  
**Scope**: SmppPduFramer Integration + Streaming PDU Parsing  

---

## Build Process

### Prerequisites
- Docker and Docker Compose installed
- Project cloned to local machine

### Build Steps

#### Step 1: Build Docker Image
```bash
cd /path/to/simple_smpp_server
docker-compose -f docker-compose.dev.yml build
```

**Expected**: Docker image `simple_smpp_server-smsc-dev:latest` created successfully

#### Step 2: Start Dev Container
```bash
docker-compose -f docker-compose.dev.yml up -d
```

**Expected**: Container `smsc-dev-container` running with:
- Port 2775 mapped to host (SMPP protocol)
- Port 2222 mapped to host (SSH access)
- Volume mounted at `/workspace` (source code)

#### Step 3: Build Server Binary Inside Container
```bash
docker exec -u developer smsc-dev-container sh -c 'cd /workspace && rm -rf build && mkdir build && cd build && cmake .. && cmake --build . --parallel 4'
```

**Expected**: 
- CMake configuration succeeds
- Binary compiled at `/workspace/build/SMPPServer/simple_smpp_server`
- No linking or compilation errors (warnings from smppcxx are acceptable)
- Build completes with: `[100%] Built target simple_smpp_server`

#### Step 4: Verify Binary Exists
```bash
docker exec smsc-dev-container ls -lh /workspace/build/SMPPServer/simple_smpp_server
```

**Expected**: Binary size ~2.8 MB, executable permissions

---

## Integration Testing

### Test Environment Setup

#### Install Test Dependencies on Host
```bash
# On your host machine (Windows, Mac, or Linux)
pip install smpplib
# or
python3 -m pip install smpplib
```

**Expected**: smpplib installed, no errors

#### Verify Port Mapping
```bash
# Verify container is running and port 2775 is mapped
docker-compose -f docker-compose.dev.yml ps

# Expected output shows:
# smsc-dev-container  0.0.0.0:2775->2775/tcp  (SMPP port mapped)
```

---

### Test Execution (Host-Based)

#### Terminal 1: Start SMPP Server in Docker

Start the server in the background so it keeps running:

```bash
docker exec -d smsc-dev-container /workspace/build/SMPPServer/simple_smpp_server --log-level=debug
```

**Or** for interactive debugging:
```bash
docker exec smsc-dev-container /workspace/build/SMPPServer/simple_smpp_server --log-level=debug
```

**Expected Output**:
```
2026-04-19 17:XX:XX.XXX [info ] [SmppServiceManager     ] initialized
2026-04-19 17:XX:XX.XXX [info ] [TcpServer              ] listening on [::]:2775
```

Server listening on port 2775 inside container (mapped to host via docker-compose).

#### Terminal 2: Run Integration Tests from Host

Change to project directory on your host machine:
```bash
cd /path/to/simple_smpp_server
```

##### Quick Test (5-minute validation)
```bash
python3 tests/quick_test.py
```

**Expected**: All steps pass
```
==================================================
Quick SMPP Server Test
==================================================

1. Connecting to 127.0.0.1:2775...
   [OK] Connected

2. Binding as TRANSMITTER...
   [OK] Bound successfully

3. Sending ENQUIRE_LINK (keep-alive)...
   [OK] Keep-alive sent, response received

4. Unbinding...
   [OK] Unbind successful

5. Disconnecting...
   [OK] Disconnected

==================================================
[PASS] ALL TESTS PASSED
==================================================
```

##### Comprehensive Test (15-minute validation)
```bash
python3 tests/test_smpp_client.py
```

**Expected**: All 6 tests pass
```
============================================================
SMPP Client Test Suite
Target: 127.0.0.1:2775
============================================================

[Test 1] BIND_TRANSMITTER         ✓ PASS
[Test 2] BIND_RECEIVER            ✓ PASS
[Test 3] BIND_TRANSCEIVER         ✓ PASS
[Test 4] AUTH_FAILURE             ✓ PASS
[Test 5] ENQUIRE_LINK_KEEPALIVE   ✓ PASS
[Test 6] DOUBLE_BIND              ✓ PASS

============================================================
TEST SUMMARY: 6/6 tests passed
============================================================
```

#### Terminal 3: Monitor Server Logs (Optional)

```bash
docker exec smsc-dev-container tail -f /var/log/simple_smpp_server/server.log
```

This shows real-time logging of PDU processing, connections, and responses.

---

## Key Test Cases

### Test 1: BIND_TRANSMITTER
- **Purpose**: Verify client can bind as sender
- **Flow**: Connect → Bind (Tx) → ENQUIRE_LINK → Unbind → Disconnect
- **Validates**: 
  - TCP connection to port 2775
  - PDU parsing (BIND_TRANSMITTER)
  - Credential validation (test/test)
  - PDU response generation
  - ENQUIRE_LINK keep-alive

### Test 2: BIND_RECEIVER
- **Purpose**: Verify client can bind as receiver
- **Flow**: Connect → Bind (Rx) → ENQUIRE_LINK → Unbind → Disconnect
- **Validates**: State machine handles BIND_RECEIVER

### Test 3: BIND_TRANSCEIVER
- **Purpose**: Verify client can bind as bidirectional
- **Flow**: Connect → Bind (Tx+Rx) → ENQUIRE_LINK → Unbind → Disconnect
- **Validates**: State machine handles BIND_TRANSCEIVER

### Test 4: AUTH_FAILURE
- **Purpose**: Verify invalid credentials are rejected
- **Flow**: Connect → Bind with wrong password
- **Validates**: Credential validation logic
- **Expected Error**: ESME_RINVPASWD (0x0000000E)

### Test 5: ENQUIRE_LINK_KEEPALIVE
- **Purpose**: Verify repeated keep-alive requests work
- **Flow**: Bind → Send 5× ENQUIRE_LINK with 0.5s delay → Unbind
- **Validates**: Multiple PDU exchanges on same connection

### Test 6: DOUBLE_BIND
- **Purpose**: Verify server rejects second BIND on same connection
- **Flow**: Bind successfully → Attempt second BIND
- **Validates**: State machine prevents double-bind
- **Expected Error**: ESME_RALYBND (0x00000005)

---

## Architecture: SmppPduFramer Integration

### Data Flow (Refactored)

```
TcpServer (async_read stream)
    ↓ feed raw bytes
SmppPduFramer (state machine: WAITING_HEADER → WAITING_BODY → PDU_COMPLETE)
    ↓ get_pdu() when complete
SmppHandler::dispatch_pdu(full_pdu, session, socket)
    ├─ Extract header (bytes 0-15)
    ├─ Parse command_id, sequence_number from header
    ├─ Extract body (bytes 16+)
    └─ Call appropriate handler:
        ├─ handle_bind_receiver(body_ptr, body_len, seq, ...)
        ├─ handle_bind_transmitter(body_ptr, body_len, seq, ...)
        ├─ handle_bind_transceiver(body_ptr, body_len, seq, ...)
        ├─ handle_unbind(seq, ...)
        └─ handle_enquire_link(seq, ...)
            ↓
        Smpp::BindReceiver(body_ptr) [smppcxx library]
            ↓
        Credential validation
            ↓
        Session state update
            ↓
        Response encoding + socket write
```

### Benefits of SmppPduFramer
1. **Proper streaming**: Handles PDUs arriving in multiple socket reads
2. **State machine**: Clear WAITING_HEADER → WAITING_BODY → PDU_COMPLETE flow
3. **No recombination**: PDU passed once to handler, not split then recombined
4. **Memory efficient**: Reusable framer per session (embedded in SmppSession)

---

## Troubleshooting

### Server Won't Start
```bash
# Check for port conflicts
docker exec smsc-dev-container lsof -i :2775

# Check for build errors
docker exec smsc-dev-container /workspace/build/SMPPServer/simple_smpp_server 2>&1 | head -20
```

### Test Connection Refused
```bash
# Verify server is listening
docker exec smsc-dev-container ss -tln | grep 2775

# Verify port is mapped
netstat -an | grep 2775

# Check IP whitelist
docker exec smsc-dev-container cat /etc/simple_smpp_server/allowed_ips.conf
```

### PDU Parsing Errors
Check logs for `[error] [SmppHandler] exception in BIND_*: ...`
- Likely cause: Invalid sequence number from smppcxx
- Fix: Verify PDU is correctly assembled in SmppPduFramer.feed_bytes()

### Authentication Failure in Tests
```bash
# Verify credentials in test file
grep -r "username\|password" tests/

# Expected: test/test
# Edit test files if using different credentials
```

---

## Success Criteria

✓ **All builds**: 
- Docker image builds without errors
- CMake configures successfully  
- Binary compiles with no errors (warnings ok)
- Binary size ~2.8 MB

✓ **All tests pass**:
- quick_test.py: All 5 steps pass
- test_smpp_client.py: All 6 tests pass (6/6)

✓ **Server logs show**:
- Proper PDU parsing: "received PDU command_id=0x..."
- Proper response dispatch: "sending BIND_*_RESP"
- Clean connection handling: "UNBIND" → "closing connection"

✓ **No crashes or segfaults** during entire test suite execution

---

## Next Steps (After Tests Pass)

1. **Phase 1.1 Completion**: Document final implementation
2. **Phase 1.2**: Implement SUBMIT_SM / DELIVER_SM operations
3. **Performance Testing**: Load testing with multiple concurrent connections
4. **Production Hardening**: Error handling, resource cleanup, graceful shutdown

---

**Status**: Ready for Integration Testing  
**Last Updated**: 2026-04-19  
**Owner**: Development Team  
