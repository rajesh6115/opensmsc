# SMPP Error Codes Reference

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: SMPP v3.4 status codes and custom error codes  

---

## SMPP v3.4 Status Codes

All status codes are 32-bit unsigned integers. Response PDU carries these codes.

### Success

| Code | Hex | Meaning | Handler Response |
|------|-----|---------|------------------|
| **ESME_ROK** | 0x00000000 | No error (success) | Return success response |

### Binding & Authentication

| Code | Hex | Meaning | When to Use |
|------|-----|---------|-------------|
| **ESME_RINVPASWD** | 0x0000000E | Invalid Password | BindHandler: auth fails |
| **ESME_RBINDFAIL** | 0x0000000D | Bind Failed | Session state change fails |
| **ESME_RALYBND** | 0x00000005 | Already Bound | BindHandler: try to bind twice |

### Invalid Requests

| Code | Hex | Meaning | When to Use |
|------|-----|---------|-------------|
| **ESME_RINVCMDID** | 0x00000008 | Invalid Command ID | SmppMessageProcessor: unknown command |
| **ESME_RINVCMDLEN** | 0x00000001 | Message Length is invalid | SmppMessageParser: PDU size invalid |
| **ESME_RINVBNDSTS** | 0x00000004 | Incorrect BIND Status | SmppConnection: not bound when required |

### Submission Errors

| Code | Hex | Meaning | When to Use |
|------|-----|---------|-------------|
| **ESME_RSUBMITFAIL** | 0x00000045 | Submit Failed | Generic processing error |
| **ESME_RINVSRCADR** | 0x0000000B | Invalid Source Address | (Future: SMS submission) |
| **ESME_RINVDSTADR** | 0x0000000C | Invalid Destination Address | (Future: SMS submission) |

### System Errors

| Code | Hex | Meaning | When to Use |
|------|-----|---------|-------------|
| **ESME_RSYSERR** | 0x00000008 | System Error | Unexpected internal error |
| **ESME_RTHROTTLED** | 0x00000058 | Throttling Error | (Future: rate limiting) |

---

## Error Code Constants

Define in `SMPPServer/include/error_codes.hpp`:

```cpp
#ifndef SMPPSERVER_ERROR_CODES_HPP_
#define SMPPSERVER_ERROR_CODES_HPP_

#include <cstdint>

namespace smpp {

namespace ErrorCode {
    // Success
    constexpr uint32_t ESME_ROK = 0x00000000;
    
    // Binding & Authentication
    constexpr uint32_t ESME_RINVPASWD = 0x0000000E;  // Invalid password
    constexpr uint32_t ESME_RBINDFAIL = 0x0000000D;  // Bind failed
    constexpr uint32_t ESME_RALYBND = 0x00000005;    // Already bound
    
    // Invalid Requests
    constexpr uint32_t ESME_RINVCMDID = 0x00000008;  // Invalid command ID
    constexpr uint32_t ESME_RINVCMDLEN = 0x00000001; // Invalid command length
    constexpr uint32_t ESME_RINVBNDSTS = 0x00000004; // Incorrect BIND status
    
    // Submission Errors
    constexpr uint32_t ESME_RSUBMITFAIL = 0x00000045; // Submit failed
    constexpr uint32_t ESME_RINVSRCADR = 0x0000000B;  // Invalid source address
    constexpr uint32_t ESME_RINVDSTADR = 0x0000000C;  // Invalid dest address
    
    // System Errors
    constexpr uint32_t ESME_RSYSERR = 0x00000008;     // System error
    constexpr uint32_t ESME_RTHROTTLED = 0x00000058;  // Throttling error
}

}  // namespace smpp

#endif  // SMPPSERVER_ERROR_CODES_HPP_
```

---

## Error Code by Handler

### BindTransmitterHandler / BindReceiverHandler / BindTransceiverHandler

```
Scenario                        → Status Code
──────────────────────────────────────────────────────────
1. Valid credentials           → ESME_ROK
2. Invalid credentials         → ESME_RINVPASWD
3. Already bound               → ESME_RALYBND
4. D-Bus unavailable           → ESME_RSUBMITFAIL
5. Session state change fails  → ESME_RBINDFAIL
6. Malformed BIND body         → ESME_RINVCMDLEN
```

**Implementation Pattern**:

```cpp
SmppMessage BindTransmitterHandler::handle(
    const SmppMessage& request,
    SmppSession& session) {
    
    // 1. Check if already bound
    if (session.is_bound()) {
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_RALYBND,
            request.sequence_number());
    }
    
    // 2. Extract and validate credentials
    try {
        auto username = request.get_system_id();
        if (username.empty()) {
            return SmppMessageEncoder::build_bind_transmitter_resp(
                ESME_RINVCMDLEN,
                request.sequence_number());
        }
    } catch (...) {
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_RINVCMDLEN,
            request.sequence_number());
    }
    
    // 3. Validate via D-Bus
    auto auth_result = dbus_auth_->authenticate(username, password, ip);
    if (!auth_result.success) {
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_RINVPASWD,
            request.sequence_number());
    }
    
    // 4. Update session state
    if (!session.try_bind_as_transmitter(username)) {
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_RBINDFAIL,
            request.sequence_number());
    }
    
    // 5. Success
    return SmppMessageEncoder::build_bind_transmitter_resp(
        ESME_ROK,
        request.sequence_number());
}
```

### UnbindHandler

```
Scenario                 → Status Code
─────────────────────────────────────
1. Unbind from BOUND     → ESME_ROK
2. Unbind from UNBOUND   → ESME_RINVBNDSTS
```

### EnquireLinkHandler

```
Scenario            → Status Code
────────────────────────────────
1. Keep-alive ok    → ESME_ROK
2. Not bound        → ESME_RINVBNDSTS
```

### SmppMessageProcessor (Router)

```
Scenario              → Status Code
──────────────────────────────────
1. Unknown command    → ESME_RINVCMDID
2. Handler exception  → ESME_RSUBMITFAIL
```

---

## Custom Internal Errors (Non-SMPP)

For non-SMPP errors (e.g., internal logging, validation):

| Error | Context | Response |
|-------|---------|----------|
| IP not whitelisted | TcpServer::on_accept() | Close socket (no SMPP response sent) |
| Logger init failed | SmppServer::start() | Log to stderr, continue (graceful degradation) |
| D-Bus connection lost | DBusAuthenticator | Return error response, trigger reconnect |
| Invalid whitelist file | IpValidator::load_whitelist() | Empty whitelist, reject all IPs |

---

## Error Code Reference Table

```cpp
// Quick lookup by description:
// "I need a code for..."

// ...invalid password
→ ESME_RINVPASWD

// ...user already bound
→ ESME_RALYBND

// ...command I don't recognize
→ ESME_RINVCMDID

// ...something unexpected broke
→ ESME_RSUBMITFAIL

// ...client not in correct state
→ ESME_RINVBNDSTS

// ...everything worked
→ ESME_ROK
```

---

## Testing Error Codes

### Unit Test Example

```cpp
TEST_CASE("Error codes are correct", "[error_codes]") {
    REQUIRE(ESME_ROK == 0x00000000);
    REQUIRE(ESME_RINVPASWD == 0x0000000E);
    REQUIRE(ESME_RALYBND == 0x00000005);
}
```

### Integration Test Example

```python
def test_invalid_credentials_returns_esme_rinvpaswd():
    client = smpplib.client.Client('127.0.0.1', 2775)
    try:
        client.connect()
        client.bind(SMPP_CLIENT_TX, system_id='test', password='wrong')
        assert False, "Should have failed"
    except smpplib.exception.SMPPException as e:
        # Check error code is ESME_RINVPASWD (14)
        assert e.status == 14
```

---

**Next**: exception_handling.md
