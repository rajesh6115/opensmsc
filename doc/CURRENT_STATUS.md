# Current Status - SMPP Server Implementation

**Date**: 2026-04-19  
**Status**: Blocking Issue - smppcxx Library Failure  

---

## Summary

The SMPP server implementation is complete with:
- ✓ Proper logging infrastructure (spdlog) 
- ✓ TCP server with IP whitelisting
- ✓ SMPP PDU parsing (header/body separation)
- ✓ Session state management
- ✓ Response generation for BIND/UNBIND/ENQUIRE_LINK

**BUT** - Testing fails with the client unable to complete BIND operation.

---

## Blocking Issue: smppcxx "Invalid sequence number"

### Symptoms
1. Client connects successfully to TCP port 2775 ✓
2. Client sends BIND_TRANSMITTER PDU ✓
3. Server receives PDU, parses header successfully ✓
4. Server attempts to parse PDU body with `Smpp::BindTransmitter(body_ptr)` ✗
   - Throws exception: "Invalid sequence number"
5. Server crashes without sending response
6. Client times out waiting for BIND_TRANSMITTER_RESP

### Current Code Flow
```cpp
// tcp_server.cpp: Successfully reads header + body
SmppHandler::dispatch_pdu(full_pdu, session, socket)

// smpp_handler.cpp: dispatch_pdu receives full PDU
dispatch_pdu(...full_pdu...)
  → Parse header: uint32_t command_id = ntoh32(full_pdu.data() + 4)
  → Parse header: uint32_t sequence_number = ntoh32(full_pdu.data() + 12)
  → Extract body pointer: body_ptr = full_pdu.data() + 16
  → handle_bind_transmitter(body_ptr, body_len, seq_num, ...)

// smpp_handler.cpp: handle_bind_transmitter
handle_bind_transmitter(const uint8_t* body_data, size_t body_len, ...)
  → Smpp::BindTransmitter bind_req(body_data)  // CRASHES HERE
      → Throws: "Invalid sequence number"
```

### Root Cause Analysis

The smppcxx library `Smpp::BindTransmitter` constructor throws "Invalid sequence number" when:

**Theory 1** (Likely): Constructor expects different data structure
- Maybe it expects full PDU (header + body), not just body?
- Maybe it expects header in a specific format?
- Current attempt: passing `full_pdu` instead of `body_ptr` made it WORSE

**Theory 2** (Possible): Sequence number validation in smppcxx
- smppcxx may have internal validation that sequence_number must be:
  - Within range 1-2^31
  - Match internal state
  - Conform to specific format

**Theory 3** (Unlikely): PDU data is corrupted
- Body pointer offset wrong
- Body data missing required fields
- Endianness issue in parsing

### What We Know
- Raw PDU from test:  `0000002300000002000000000000000174657374007465737400534d50500034010100`
  - command_length: 0x00000023 (35 bytes) ✓
  - command_id: 0x00000002 (BIND_TRANSMITTER) ✓
  - command_status: 0x00000000 ✓
  - sequence_number: 0x00000001 ✓
  - body (19 bytes): `74657374007465737400534d50500034010100` 
    - `74657374 00` = "test\0"
    - `74657374 00` = "test\0"
    - `534d5050 00` = "SMPP\0"
    - `34 01 01 00` = version(0x34), addr_ton(1), addr_npi(1), address_range(empty)

**The body data looks correct**

---

## Next Steps to Fix

### Option A: Debug smppcxx Constructor
1. Add hex dump logging of body_ptr and body_len before calling constructor
2. Try constructing with different offsets (maybe body should include part of header?)
3. Check smppcxx library source for "Invalid sequence number" error message
4. Determine what smppcxx constructor actually expects

### Option B: Use Different SMPP Library
- If smppcxx is fundamentally broken, consider:
  - Creating custom SMPP PDU parser for BIND commands
  - Using a different C++ SMPP library
  - Implementing SMPP message parsing manually

### Option C: Workaround
- Implement custom BIND handler that doesn't use smppcxx for parsing
- Use only smppcxx for response generation

---

## Test Readiness

✓ Build system works (clean rebuild successful)  
✓ Docker infrastructure ready  
✓ Host-based testing infrastructure ready (python + smpplib installed)  
✓ Port mapping configured (2775)  
✗ Server cannot complete BIND - **CRITICAL BLOCKER**

---

## Code Locations

- SMPP Handler: `/workspace/SMPPServer/src/smpp_handler.cpp` (lines 75-142)
- TCP Server: `/workspace/SMPPServer/src/tcp_server.cpp` (body reading at lines 145-173)
- Test: `/workspace/tests/quick_test.py` (line 35: bind_transmitter call)

---

## Reproduction Steps

1. `docker exec -d smsc-dev-container /workspace/build/SMPPServer/simple_smpp_server`
2. Wait 2 seconds
3. `python tests/quick_test.py`
4. Observe: Connection OK, Bind fails with "ConnectionError"

---

**Owner**: Development Team  
**Priority**: CRITICAL - Blocks all functional testing  
**Next Action**: Debug smppcxx library behavior  
