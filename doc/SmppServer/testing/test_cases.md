# Specific Test Cases Per Class

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: Detailed test cases for each class  

---

## SmppMessage

### Happy Path
- [ ] Construct with command_id, status, sequence_number, body
- [ ] Accessors return correct values
- [ ] is_bind_transmitter() returns true for cmd_id 0x00000002
- [ ] is_bind_receiver() returns true for cmd_id 0x00000001
- [ ] is_bind_transceiver() returns true for cmd_id 0x00000009
- [ ] is_bind_response() returns true for cmd_id | 0x80000000
- [ ] is_unbind() returns true for cmd_id 0x00000006
- [ ] is_enquire_link() returns true for cmd_id 0x0000000F

### Serialization
- [ ] encode() produces 16-byte header + body
- [ ] encode() produces big-endian command_id/status/sequence_number
- [ ] decode() inverts encode() correctly (roundtrip)
- [ ] decode() with short PDU (< 16 bytes) throws std::invalid_argument
- [ ] Empty body encodes to exactly 16 bytes

### Field Extraction
- [ ] get_system_id() extracts C-string from body
- [ ] get_system_id() returns empty string if no null terminator
- [ ] get_password() extracts second C-string from body

---

## SmppSession

### State Initialization
- [ ] Constructed in UNBOUND state
- [ ] session_id() and client_ip() are set correctly
- [ ] authenticated_as() is empty initially
- [ ] is_bound() returns false initially

### State Transitions - Valid
- [ ] UNBOUND → BOUND_TRANSMITTER via try_bind_as_transmitter("user") returns true
- [ ] UNBOUND → BOUND_RECEIVER via try_bind_as_receiver("user") returns true
- [ ] UNBOUND → BOUND_TRANSCEIVER via try_bind_as_transceiver("user") returns true
- [ ] BOUND_* → UNBOUND via try_unbind() returns true
- [ ] Transition updates authenticated_as()
- [ ] can_transmit() returns true only in BOUND_TRANSMITTER or BOUND_TRANSCEIVER
- [ ] can_receive() returns true only in BOUND_RECEIVER or BOUND_TRANSCEIVER

### State Transitions - Invalid
- [ ] BOUND_TRANSMITTER → BIND again returns false (no state change)
- [ ] BOUND_RECEIVER → BIND again returns false
- [ ] BOUND_TRANSCEIVER → BIND again returns false
- [ ] UNBOUND → UNBIND returns false (no state change)
- [ ] CLOSED → any transition returns false

### Activity Tracking
- [ ] last_activity() changes after touch()
- [ ] last_activity() time is reasonable (std::time(nullptr) value)

### Thread Safety
- [ ] Multiple readers can call is_bound() concurrently
- [ ] Reader blocked while writer modifies state
- [ ] Writer gets exclusive access to state_

---

## SmppMessageParser

### Header Parsing
- [ ] parse_bytes() with exactly 16 bytes (header only) → has_complete_pdu() = true
- [ ] parse_bytes() with 10 bytes (partial header) → has_complete_pdu() = false
- [ ] bytes_needed() returns remaining bytes needed
- [ ] Extracts command_length correctly from header bytes [0:4]

### Body Parsing
- [ ] parse_bytes() with header + complete body → returns 1 SmppMessage
- [ ] parse_bytes() with header + partial body → waits for more
- [ ] parse_bytes() with multiple complete PDUs → returns multiple SmppMessages

### Chunked Input
- [ ] parse_bytes called multiple times with fragments → reassembles correctly
- [ ] First call: 8 bytes of header
- [ ] Second call: 8 bytes of header + 4 bytes of body
- [ ] Third call: remaining body bytes
- [ ] Result: complete, correct message

### Edge Cases
- [ ] Zero-length body (command_length = 16)
- [ ] Large body (max reasonable size)
- [ ] Partial header followed by immediate body
- [ ] Multiple calls with partial data then complete

---

## SmppMessageEncoder

### Response Building
- [ ] build_bind_transmitter_resp(ESME_ROK, 1) returns command_id 0x80000002
- [ ] build_bind_transmitter_resp status is set correctly
- [ ] build_bind_transmitter_resp sequence_number matches
- [ ] build_bind_receiver_resp returns command_id 0x80000001
- [ ] build_bind_transceiver_resp returns command_id 0x80000009
- [ ] build_unbind_resp returns command_id 0x80000006
- [ ] build_enquire_link_resp returns command_id 0x8000000F

### Error Status Codes
- [ ] ESME_ROK (0x00000000) = success
- [ ] ESME_RINVPASWD (0x0000000E) = invalid password
- [ ] ESME_RALYBND (0x00000005) = already bound
- [ ] ESME_RBINDFAIL (0x0000000D) = bind failed
- [ ] ESME_RSUBMITFAIL (0x00000045) = submit failed

### Body Format
- [ ] BIND responses include system_id as C-string
- [ ] UNBIND/ENQUIRE_LINK responses have empty body

---

## IpValidator

### Whitelist Management
- [ ] load_whitelist() reads file and populates internal set
- [ ] File with IPv4 addresses: "127.0.0.1\n192.168.1.1"
- [ ] File with IPv6 addresses: "::1\n2001:db8::1"
- [ ] File with comments (lines starting with #) are ignored
- [ ] Empty lines are ignored
- [ ] add_ip() adds single IP to whitelist

### IP Validation
- [ ] is_whitelisted("127.0.0.1") returns true if in file
- [ ] is_whitelisted("192.168.1.50") returns false if not in file
- [ ] is_whitelisted() handles IPv6 correctly
- [ ] is_whitelisted("invalid.format") returns false
- [ ] Empty whitelist: is_whitelisted() always returns false
- [ ] is_whitelisted() case-sensitive

### CIDR Support (Optional for v1)
- [ ] load_whitelist("192.168.0.0/24") supports CIDR notation
- [ ] is_whitelisted("192.168.0.50") matches "192.168.0.0/24"
- [ ] is_whitelisted("192.169.0.1") does NOT match "192.168.0.0/24"

---

## CredentialValidator

### Format Validation
- [ ] validate_credentials("user", "pass") returns valid=true
- [ ] validate_credentials("", "pass") returns valid=false
- [ ] validate_credentials("user", "") returns valid=false
- [ ] validate_credentials("a"*33, "pass") returns valid=false (username too long)
- [ ] validate_credentials("user_123", "pass") returns valid=true (alphanumeric + underscore)
- [ ] validate_credentials("user-123", "pass") returns valid=false (dash not allowed)

### C-String Extraction
- [ ] extract_from_bind_body([u,s,e,r,\0,p,a,s,s,\0]) → username="user", password="pass"
- [ ] extract_from_bind_body(empty) → valid=false
- [ ] extract_from_bind_body(no null terminator) → valid=false
- [ ] extract_from_bind_body correctly handles binary body

---

## DBusAuthenticator

### Successful Authentication
- [ ] authenticate("test", "pass", "127.0.0.1") returns success=true (if service responds OK)
- [ ] authenticate() makes D-Bus method call to /com/telecom/SMPPAuthenticator
- [ ] D-Bus call passes username, password, client_ip

### Failed Authentication
- [ ] authenticate("test", "wrong", "127.0.0.1") returns success=false

### Error Handling
- [ ] D-Bus service unavailable → returns success=false, message contains error
- [ ] D-Bus method timeout → returns success=false, message contains error
- [ ] D-Bus invalid interface → returns success=false

---

## Logger

### Initialization
- [ ] init() creates stdout sink
- [ ] init() creates rotating file sink
- [ ] Both sinks are configured with same pattern
- [ ] Pattern includes timestamp, level, and message

### Logging Levels
- [ ] init(..., "debug") sets level to DEBUG
- [ ] init(..., "info") sets level to INFO
- [ ] init(..., "warn") sets level to WARN
- [ ] init(..., "error") sets level to ERROR
- [ ] init(..., "critical") sets level to CRITICAL
- [ ] Invalid level defaults to INFO

### Dual Sinks
- [ ] Message logged to stdout (visible in console)
- [ ] Message logged to file (/var/log/simple_smpp_server/server.log)
- [ ] Both logs have same format and timestamp

### Level Filtering
- [ ] INFO level: debug messages NOT logged, info+ messages logged
- [ ] ERROR level: debug, info, warn NOT logged, error+ logged

---

## SessionManager

### Session Management
- [ ] create_session("127.0.0.1") returns SmppSession with unique session_id
- [ ] create_session() called twice → different session_ids
- [ ] get_session(id) returns session if exists, nullptr if not
- [ ] remove_session(id) deletes session
- [ ] get_session(id) after remove → nullptr

### Queries
- [ ] active_count() returns number of active sessions
- [ ] get_all_sessions() returns vector of all sessions
- [ ] get_bound_sessions() returns only BOUND_* sessions

### Cleanup
- [ ] cleanup_idle_sessions(threshold) removes sessions idle > threshold
- [ ] Session with last_activity < (now - threshold) is removed
- [ ] Session with recent activity is kept
- [ ] close_all_sessions() removes all

### Thread Safety
- [ ] Multiple threads can call get_session() concurrently
- [ ] create/remove serialized under lock

---

## BindTransmitterHandler

### Happy Path
- [ ] handle() with valid credentials → returns response with ESME_ROK
- [ ] Response has command_id 0x80000002 (BIND_TRANSMITTER_RESP)
- [ ] Session state changed to BOUND_TRANSMITTER
- [ ] session.authenticated_as() == username

### Error Cases
- [ ] handle() on already bound session → ESME_RALYBND
- [ ] handle() with invalid credentials → ESME_RINVPASWD
- [ ] handle() with D-Bus unavailable → ESME_RSUBMITFAIL
- [ ] handle() with malformed body → ESME_RINVPASWD or exception

---

## UnbindHandler

### Happy Path
- [ ] handle() when BOUND_TRANSMITTER → ESME_ROK, state → UNBOUND
- [ ] handle() when BOUND_RECEIVER → ESME_ROK, state → UNBOUND
- [ ] handle() when BOUND_TRANSCEIVER → ESME_ROK, state → UNBOUND

### Error Cases
- [ ] handle() when UNBOUND → ESME_RINVSTATE or similar error code

---

## EnquireLinkHandler

### Happy Path
- [ ] handle() when BOUND → ESME_ROK
- [ ] No state change (still BOUND)

### Error Cases
- [ ] handle() when UNBOUND → error code

---

## SmppMessageProcessor

### Routing
- [ ] BIND_TRANSMITTER (0x00000002) → BindTransmitterHandler
- [ ] BIND_RECEIVER (0x00000001) → BindReceiverHandler
- [ ] BIND_TRANSCEIVER (0x00000009) → BindTransceiverHandler
- [ ] UNBIND (0x00000006) → UnbindHandler
- [ ] ENQUIRE_LINK (0x0000000F) → EnquireLinkHandler
- [ ] Unknown command → error response

### Exception Handling
- [ ] Handler throws std::exception → returns error response, not propagated

---

## Integration Tests

### SmppConnection + ASIO
- [ ] Connection receives header → reads body → dispatches
- [ ] Receives response → writes to socket

### TcpServer + SmppConnection
- [ ] Server accepts connection with whitelisted IP
- [ ] Server rejects connection with non-whitelisted IP
- [ ] Multiple concurrent connections handled

### Full BIND Flow (E2E)
- [ ] Client connects → server accepts
- [ ] Client sends BIND_TRANSMITTER → server processes
- [ ] Server validates credentials via D-Bus
- [ ] Server sends BIND_TRANSMITTER_RESP with ESME_ROK
- [ ] Client receives response

---

**Next**: mock_objects.md
