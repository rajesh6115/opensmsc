# SMPP Protocol Design & Implementation Guide

**Date**: 2026-04-19  
**Status**: Design Phase (Step 1.1.3)  
**Version**: v1.0  

---

## 1. SMPP v3.4 Protocol Overview

### 1.1 Protocol Basics
- **Name**: Short Message Service Peer-to-Peer Protocol
- **Version**: 3.4 (RFC)
- **Transport**: TCP/IP
- **Default Port**: 2775 (SMPP), 2776 (SMPP with TLS)
- **Data Format**: Binary (big-endian network byte order)
- **Session Model**: Connection-oriented (long-lived TCP connection)

### 1.2 Key Concepts
- **PDU (Protocol Data Unit)**: Atomic message (binary blob with header + body)
- **Command**: Operation type (BIND, SUBMIT_SM, DELIVER_SM, etc.)
- **Sequence Number**: Unique per-session counter (0-2^31-1) for request/response pairing
- **Status Code**: Response status (0=success, non-zero=error)

---

## 2. SMPP PDU Structure

### 2.1 PDU Header (16 bytes, fixed)
All SMPP PDUs start with identical 16-byte header:

```
Offset  Length  Field               Byte Order  Description
0       4       command_length      Network     Total PDU length (header + body)
4       4       command_id          Network     Operation code (0x00000001=BIND_RECEIVER, etc.)
8       4       command_status      Network     Status code (0=success, error codes)
12      4       sequence_number     Network     Request/response matching number
```

### 2.2 SMPP Command IDs (Relevant for MVP)

| Command | Code | Type | Direction | Description |
|---------|------|------|-----------|-------------|
| BIND_RECEIVER | 0x00000001 | Request | Client → Server | Authenticate as receiver |
| BIND_TRANSMITTER | 0x00000002 | Request | Client → Server | Authenticate as transmitter |
| BIND_TRANSCEIVER | 0x00000009 | Request | Client → Server | Authenticate as both RX+TX |
| BIND_RECEIVER_RESP | 0x80000001 | Response | Server → Client | BIND_RECEIVER success/fail |
| BIND_TRANSMITTER_RESP | 0x80000002 | Response | Server → Client | BIND_TRANSMITTER success/fail |
| BIND_TRANSCEIVER_RESP | 0x80000009 | Response | Server → Client | BIND_TRANSCEIVER success/fail |
| UNBIND | 0x00000006 | Request | Client → Server | Graceful disconnect |
| UNBIND_RESP | 0x80000006 | Response | Server → Client | Acknowledge unbind |
| ENQUIRE_LINK | 0x0000000F | Request | Both | Keep-alive heartbeat |
| ENQUIRE_LINK_RESP | 0x8000000F | Response | Both | Keep-alive response |

**Pattern**: Response codes = Request code + 0x80000000

### 2.3 Response Status Codes

| Status | Code | Meaning |
|--------|------|---------|
| Success | 0x00000000 | Operation successful |
| Invalid Length | 0x00000001 | Message has invalid length |
| Invalid Command Length | 0x00000002 | Command length field invalid |
| Invalid Command ID | 0x00000003 | Unknown command_id |
| Incorrect Bind Status | 0x00000004 | Not bound (must BIND first) |
| Already Bound | 0x00000005 | Already bound (duplicate BIND) |
| Invalid Bind Status for Msg Type | 0x00000006 | Operation not allowed in current mode |
| ESME Authentication Failure | 0x00000008 | Invalid username/password |
| Server Temporarily Unavailable | 0x00000058 | Server overloaded/unavailable |

---

## 3. SMPP PDU Body Structure (MVP Operations)

### 3.1 BIND_* Request Body

```
Field               Type        Length  Notes
system_id           C-Octet     1-16    Client username
password            C-Octet     1-16    Client password
system_type         C-Octet     1-13    "ESME" or empty (typical)
interface_version   Octet       1       SMPP version (0x34 = v3.4)
addr_ton            Octet       1       Type of Number (typically 0 or 1)
addr_npi            Octet       1       Numbering Plan (typically 0 or 1)
address_range       C-Octet     1-21    Address range (empty for MVP)
```

**C-Octet**: Null-terminated ASCII string (length not specified in header)

### 3.2 BIND_* Response Body

```
Field               Type        Length  Notes
system_id           C-Octet     1-16    Server system ID (can be same as request)
```

### 3.3 UNBIND Request Body
- **Empty** (header only, no body)

### 3.3 UNBIND Response Body
- **Empty** (header only, no body)

### 3.4 ENQUIRE_LINK Request Body
- **Empty** (header only, no body)

### 3.5 ENQUIRE_LINK_RESP Body
- **Empty** (header only, no body)

---

## 4. PDU Parsing & Generation Flow

### 4.1 Client-side Flow (Simplified)
```
Client Application
    ↓
[Create BIND_TRANSMITTER PDU]
    ↓
[Serialize to binary (using smppcxx)]
    ↓
[Write to TCP socket]
    ↓
[Read 16-byte header from socket]
    ↓
[Read command_length - 16 bytes for body]
    ↓
[Deserialize BIND_TRANSMITTER_RESP PDU (using smppcxx)]
    ↓
[Check status code]
    ↓
Connection established (authenticated)
```

### 4.2 Server-side Flow (Phase 1.1)
```
TCP Server
    ↓ (accept connection)
IP Validation & Session creation
    ↓
[async_read: 16-byte header]
    ↓
[Extract command_length]
    ↓
[async_read: remaining body bytes]
    ↓
SmppServiceManager::handle_pdu(buffer, length)
    ↓
[Identify command_id]
    ↓
[Dispatch to handler: handle_bind_transmitter / handle_bind_receiver / etc.]
    ↓
[Generate response PDU (using smppcxx)]
    ↓
[Serialize response]
    ↓
[async_write: response to socket]
    ↓
[Keep connection open for next PDU or UNBIND]
```

---

## 5. smppcxx Library API Usage

### 5.1 Library Overview
- **Location**: `external/smppcxx/`
- **Namespace**: `Smpp::`
- **Classes**: BindReceiver, BindTransmitter, BindTransceiver, Unbind, EnquireLink, etc.
- **Common Methods**:
  - Constructor from buffer: `BindReceiver(const Smpp::Uint8* buffer)`
  - Serialization: `body()` → buffer
  - Field accessors: `system_id()`, `password()`, etc.

### 5.2 Example: Parse BIND_RECEIVER

```cpp
// buffer = raw bytes from TCP socket
const Smpp::Uint8* pdu_body = buffer + 16;  // skip 16-byte header
Smpp::BindReceiver bind_req(pdu_body);

std::string system_id = bind_req.system_id().c_str();
std::string password = bind_req.password().c_str();
std::string system_type = bind_req.system_type().c_str();
```

### 5.3 Example: Generate BIND_TRANSMITTER_RESP

```cpp
#include "smpp.hpp"

// Create response
Smpp::BindTransmitterResp bind_resp;
bind_resp.command_status(Smpp::CommandStatus::ESOK);  // success
bind_resp.sequence_number(sequence_number);  // from request
bind_resp.system_id("TestServer");

// Serialize to buffer
std::vector<Smpp::Uint8> resp_buffer = bind_resp.get();

// Send via TCP
asio::write(socket, asio::buffer(resp_buffer), ec);
```

---

## 6. Client Session State Machine

### 6.1 States

```
UNBOUND
    ↓ [Receive BIND_*]
    ├─ [Valid credentials] → BOUND_TX or BOUND_RX or BOUND_TXRX
    └─ [Invalid credentials] → send BIND_*_RESP(error) → UNBOUND
         
BOUND_TX
    ↓ [Receive UNBIND]
    ├─ [Send UNBIND_RESP] → UNBOUND → close
    
BOUND_RX / BOUND_TXRX
    ↓ [Receive ENQUIRE_LINK]
    ├─ [Send ENQUIRE_LINK_RESP] → stay in same state
```

### 6.2 Session Lifecycle Per Client Connection

1. **UNBOUND** (initial state)
   - Accept TCP connection
   - Receive BIND_TRANSMITTER/RECEIVER/TRANSCEIVER
   - Validate credentials
   - Send BIND_*_RESP (success/error)
   - Transition to BOUND or close

2. **BOUND** (authenticated state)
   - Receive ENQUIRE_LINK → send ENQUIRE_LINK_RESP (keep-alive)
   - Receive UNBIND → send UNBIND_RESP, close connection
   - Receive SUBMIT_SM → send SUBMIT_SM_RESP (Phase 1.2+)
   - Receive DELIVER_SM → send DELIVER_SM_RESP (Phase 1.2+)

3. **CLOSED** (end state)
   - TCP connection closed
   - Session resources freed

---

## 7. Implementation Architecture

### 7.1 New Components Needed

```
SMPPServer/
├── include/
│   ├── tcp_server.hpp              ← REDESIGN for SMPP
│   ├── smpp_handler.hpp            ← NEW: handles PDU dispatch
│   ├── smpp_session.hpp            ← NEW: per-connection state
│   └── smpp_protocol.hpp           ← NEW: helper functions
├── src/
│   ├── tcp_server.cpp              ← REDESIGN for SMPP
│   ├── smpp_handler.cpp            ← NEW: implementation
│   ├── smpp_session.cpp            ← NEW: session state
│   └── smpp_protocol.cpp           ← NEW: protocol helpers
└── tests/
    └── test_smpp_operations.cpp    ← NEW: unit tests
```

### 7.2 Redesigned TcpServer

**Current (wrong)**:
```cpp
// Sends text: "OK <session_id>\n"
// Closes after sending
```

**New (SMPP-compliant)**:
```cpp
void handle_connection(asio::ip::tcp::socket socket) {
    auto session = std::make_shared<SmppSession>();
    
    // Start async read loop
    read_pdu_header(socket);  // async
}

void on_pdu_header_received(std::vector<uint8_t> header) {
    // Parse command_length from header
    SmppHandler::dispatch(header, body_buffer);
}
```

### 7.3 SMPP Handler Component

```cpp
class SmppHandler {
public:
    static void dispatch(const std::vector<uint8_t>& pdu_header,
                        const std::vector<uint8_t>& pdu_body,
                        SmppSession& session,
                        TcpServer& server);
                        
private:
    static void handle_bind_transmitter(...);
    static void handle_bind_receiver(...);
    static void handle_bind_transceiver(...);
    static void handle_unbind(...);
    static void handle_enquire_link(...);
};
```

### 7.4 SMPP Session Component

```cpp
class SmppSession {
public:
    enum class State { UNBOUND, BOUND_TX, BOUND_RX, BOUND_TXRX };
    
    State state() const;
    void bind(const std::string& system_id, State new_state);
    void unbind();
    uint32_t allocate_sequence_number();
    
private:
    State state_;
    std::string system_id_;
    uint32_t sequence_counter_;
};
```

---

## 8. Protocol Validation Rules

### 8.1 Command Validation
- ✅ BIND_* → UNBOUND state (valid)
- ❌ BIND_* → BOUND state (error: ESME_RALYBND)
- ✅ UNBIND → BOUND state (valid)
- ❌ UNBIND → UNBOUND state (error: ESME_RNOTBND)
- ✅ ENQUIRE_LINK → BOUND state (valid)
- ❌ ENQUIRE_LINK → UNBOUND state (error: ESME_RNOTBND)

### 8.2 Credentials Validation
- Simple username/password match (Phase 1.1)
- Default credentials: `username="test"`, `password="test"`
- Error on mismatch: send BIND_*_RESP with status=ESME_AUTHENTICATION_FAIL

### 8.3 Sequence Number Handling
- Allocate per-session counter (starting at 1)
- Increment for each request
- Response uses same sequence_number as request
- Allows client to match responses to requests

---

## 9. TCP Socket Handling

### 9.1 Reading PDUs
```cpp
// Step 1: Read header (16 bytes)
async_read(socket, buffer(header, 16), [this](ec, bytes) {
    if (ec) return;  // error
    
    // Step 2: Extract command_length from header
    uint32_t total_length = ntoh32(header[0]);
    uint32_t body_length = total_length - 16;
    
    // Step 3: Read body
    async_read(socket, buffer(body, body_length), [this](ec, bytes) {
        on_pdu_complete(header, body);
        read_next_pdu();  // queue next read
    });
});
```

### 9.2 Writing PDUs
```cpp
// Create response PDU using smppcxx
std::vector<uint8_t> response = pdu.get();

async_write(socket, buffer(response), [this](ec, bytes) {
    if (ec) {
        close_connection();
        return;
    }
    // Continue processing
});
```

### 9.3 Connection Closure
- Explicit: UNBIND_RESP sent, then close()
- Implicit: socket error, IP validation failure
- Timeout: Session inactivity (Phase 2+)

---

## 10. Credentials (MVP Phase)

### 10.1 Hardcoded Credentials
For MVP (Phase 1.1), accept any username/password combination that matches default:

```cpp
const std::string ALLOWED_USERNAME = "test";
const std::string ALLOWED_PASSWORD = "test";

bool validate_credentials(const std::string& username,
                         const std::string& password) {
    return (username == ALLOWED_USERNAME) &&
           (password == ALLOWED_PASSWORD);
}
```

### 10.2 Future (Phase 2+)
- Database lookup (SQLite)
- LDAP authentication
- Token-based validation

---

## 11. Testing Strategy

### 11.1 Unit Tests
- PDU serialization/deserialization
- State transitions
- Credential validation
- Error response generation

### 11.2 Integration Tests
- Full connection lifecycle: BIND → ENQUIRE_LINK → UNBIND
- Invalid credentials handling
- Sequence number tracking
- Multiple concurrent clients

### 11.3 Manual Testing (Using SMPP Client)

```bash
# Option 1: Using netcat + xxd (hex dump)
nc localhost 2775 < binary_pdu.bin | xxd

# Option 2: Using Python smpplib
pip install smpplib
python3 -c "
import smpplib.client
client = smpplib.client.Client('127.0.0.1', 2775)
client.connect()
client.bind_transmitter(system_id='test', password='test')
client.unbind()
client.disconnect()
"

# Option 3: Using SMPP CLI tools (if available)
smppp -H localhost -P 2775 -u test -p test
```

---

## 12. Key Takeaways

1. **SMPP is binary protocol**: Not text-based like HTTP
2. **PDU structure**: Fixed 16-byte header + variable body
3. **Connection model**: Long-lived TCP, multiple PDUs per connection
4. **State machine**: Client must BIND before other operations
5. **smppcxx library**: Handles serialization/deserialization
6. **Async architecture**: ASIO's async_read/async_write required

---

## Next Steps (Step 1.1.4)

Implement SMPP BIND operations:
1. Redesign TcpServer to handle SMPP PDUs
2. Create SmppSession and SmppHandler classes
3. Implement BIND_TRANSMITTER/RECEIVER/TRANSCEIVER handling
4. Send appropriate response PDUs
5. Test with SMPP client

---

## References

- [SMPP v3.4 Specification](https://smpp.org/docs/SMPP_v3_4.pdf)
- smppcxx: `external/smppcxx/include/smpp.hpp`
- Header structure: `external/smppcxx/include/header.hpp`
- Command IDs: `external/smppcxx/include/command_id.hpp`
- Status codes: `external/smppcxx/include/command_status.hpp`

---

**Document Status**: Design complete, ready for Step 1.1.4 implementation
