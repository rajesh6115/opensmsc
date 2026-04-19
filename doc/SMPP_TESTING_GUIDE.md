# SMPP Server Testing Guide

**Date**: 2026-04-19  
**Target**: Phase 1.1 - SMPP BIND/UNBIND/ENQUIRE_LINK Testing  
**Status**: Ready to Test

---

## Quick Start (5 minutes)

### 1. Install Python SMPP Client Library

```bash
# Option 1: From PyPI (recommended)
pip install smpplib

# Option 2: From source
git clone https://github.com/python-smpplib/python-smpplib.git
cd python-smpplib
pip install -e .
```

### 2. Start SMPP Server (in Docker container)

```bash
# In host terminal
docker exec smsc-dev-container bash -c '/workspace/build/SMPPServer/simple_smpp_server'
```

**Expected output:**
```
[INFO] simple_smpp_server starting
[INFO]   port      : 2775
[INFO]   ip_config : /etc/simple_smpp_server/allowed_ips.conf
[INFO] IpValidator: loaded 1 exact entries and 0 CIDR ranges
[INFO] SmppServiceManager: initialized (Phase 1.1 MVP)
[INFO] TcpServer: listening on [::]:2775
[INFO] io_context running on N thread(s)
```

### 3. Run Test Suite (in another terminal)

```bash
# From project root
python tests/test_smpp_client.py

# Or with custom host/port
python tests/test_smpp_client.py 127.0.0.1 2775
```

**Expected output:**
```
============================================================
SMPP Client Test Suite
Target: 127.0.0.1:2775
============================================================

============================================================
TEST 1: BIND_TRANSMITTER
============================================================
[✓] Connected to 127.0.0.1:2775
[✓] Socket connected
[✓] BIND_TRANSMITTER successful (username: test, password: test)
[✓] ENQUIRE_LINK sent
[✓] UNBIND successful
[✓] Disconnected

... (more tests) ...

============================================================
TEST SUMMARY
============================================================
BIND_TRANSMITTER              ✓ PASS
BIND_RECEIVER                 ✓ PASS
BIND_TRANSCEIVER              ✓ PASS
AUTH_FAILURE                  ✓ PASS
ENQUIRE_LINK_KEEPALIVE        ✓ PASS
DOUBLE_BIND                   ✓ PASS
============================================================
Result: 6/6 tests passed
============================================================
```

---

## Test Suite Details

### Test 1: BIND_TRANSMITTER
**What it tests**: Client binding as transmitter (sender)
- Connect to server
- Send BIND_TRANSMITTER PDU with username=test, password=test
- Send ENQUIRE_LINK to verify connection is active
- Unbind gracefully
- Disconnect

**Expected**: ✓ PASS

---

### Test 2: BIND_RECEIVER
**What it tests**: Client binding as receiver (listener)
- Connect to server
- Send BIND_RECEIVER PDU with username=test, password=test
- Send ENQUIRE_LINK
- Unbind gracefully
- Disconnect

**Expected**: ✓ PASS

---

### Test 3: BIND_TRANSCEIVER
**What it tests**: Client binding as both transmitter and receiver
- Connect to server
- Send BIND_TRANSCEIVER PDU
- Send ENQUIRE_LINK
- Unbind gracefully
- Disconnect

**Expected**: ✓ PASS

---

### Test 4: AUTHENTICATION FAILURE
**What it tests**: Server rejects invalid credentials
- Connect to server
- Attempt BIND_TRANSMITTER with wrong password
- Should receive error response with status ESME_RINVPASWD
- Connection closes

**Expected**: ✓ PASS (error handled correctly)

---

### Test 5: ENQUIRE_LINK KEEP-ALIVE
**What it tests**: Server responds to multiple keep-alive requests
- Bind to server
- Send 5 ENQUIRE_LINK PDUs with 0.5s delay between each
- Each should get ENQUIRE_LINK_RESP response
- Unbind and disconnect

**Expected**: ✓ PASS (all 5 keep-alives succeed)

---

### Test 6: DOUBLE BIND
**What it tests**: Server rejects second BIND on same connection
- Bind to server (success)
- Attempt second BIND on same connection
- Should receive error response with status ESME_RALYBND
- Connection closes

**Expected**: ✓ PASS (error handled correctly)

---

## Python SMPP Client Libraries

### Recommended: python-smpplib
- **GitHub**: [python-smpplib/python-smpplib](https://github.com/python-smpplib/python-smpplib)
- **PyPI**: [smpplib](https://pypi.org/project/smpplib/)
- **Status**: Active, Production/Stable
- **Python Support**: 2.7, 3.9+
- **Features**:
  - Full SMPP v3.4 support
  - Bind (Transmitter/Receiver/Transceiver)
  - UNBIND
  - ENQUIRE_LINK
  - Custom sequence number generators
  - Clean Pythonic API

### Alternative: smpp-client Libraries
Other options available on [GitHub SMPP Client Topics](https://github.com/topics/smpp-client)

---

## Manual Testing (Without Python)

### Using netcat + xxd (Linux/Mac/Docker)

```bash
# 1. Create BIND_TRANSMITTER PDU (raw bytes)
# Note: This is advanced - requires manual PDU construction

# 2. Send via netcat
nc -w 5 127.0.0.1 2775 < bind_pdu.bin | xxd
```

### Using telnet (Basic connectivity test)

```bash
# Note: SMPP is binary protocol, telnet won't show readable output
telnet 127.0.0.1 2775
# Just verifies TCP connection works
```

---

## Test Results Interpretation

### All 6 Tests Pass ✓
→ Server correctly implements:
- BIND_TRANSMITTER/RECEIVER/TRANSCEIVER
- Credential validation
- UNBIND graceful disconnect
- ENQUIRE_LINK keep-alive
- State machine (can't double bind)

### Some Tests Fail
Check error messages:
- **Connection refused**: Server not running, check port 2775
- **Authentication error**: Verify credentials (test/test for MVP)
- **Unexpected error**: Check server logs for error details

### Server Logs During Testing

```
[INFO] TcpServer: connection from 127.0.0.1:xxxxx
[INFO] TcpServer: IP 127.0.0.1 is allowed
[INFO] TcpServer: session_id=...
[INFO] SmppHandler: received PDU command_id=0x2 seq=1
[INFO] SmppHandler: BIND_TRANSMITTER username=test
[INFO] SmppHandler: sending BIND_TRANSMITTER_RESP
[INFO] SmppHandler: received PDU command_id=0xf seq=2
[INFO] SmppHandler: ENQUIRE_LINK keep-alive
[INFO] SmppHandler: sending ENQUIRE_LINK_RESP
[INFO] SmppHandler: received PDU command_id=0x6 seq=3
[INFO] SmppHandler: UNBIND
[INFO] SmppHandler: sending UNBIND_RESP and closing connection
```

---

## Troubleshooting

### "Connection refused" Error
```
[✗] Error: [Errno 111] Connection refused
```
**Solution**: Make sure server is running
```bash
docker exec smsc-dev-container ps aux | grep simple_smpp
```

### "Authentication failed" Error
```
[✗] Error: SMPP authentication failed
```
**Solution**: MVP uses hardcoded credentials:
- Username: `test`
- Password: `test`

### "Already bound" Error
```
[✗] Error: Already bound status
```
**Solution**: This is expected for test 6 (double bind) - it tests rejection

### Server Crashes
Check logs for:
- Protocol parsing errors
- Segmentation faults
- Memory issues

```bash
# Check server logs
docker logs smsc-dev-container
```

---

## Next Steps

### If All Tests Pass ✓
1. Continue to Step 1.1.8: Documentation & Commit
2. Prepare Phase 1.1 completion report
3. Plan Phase 1.2: Message Operations (SUBMIT_SM, DELIVER_SM)

### If Tests Fail
1. Check error details in output
2. Review server logs
3. Check specific handler code (e.g., smpp_handler.cpp)
4. Debug and fix issues
5. Re-run tests

---

## References

- [SMPP v3.4 Specification](https://en.wikipedia.org/wiki/Short_Message_Peer-to-Peer)
- [python-smpplib Documentation](https://github.com/python-smpplib/python-smpplib)
- [SMPP Protocol Overview](melroselabs.com/docs/tutorials/sms/send-sms-with-smpp-using-python/)

---

**Status**: Ready for Phase 1.1 Testing and Verification
