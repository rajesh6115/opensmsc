# Class Implementation Checklist

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: Implementation order and per-class checklist  

---

## Implementation Order (with Rationale)

### Phase 1A: Foundation (State Machine & Protocol Adapter)
These have minimal dependencies and must be done first.

**1. SmppSession** (business_logic_layer.md)
- Dependencies: None (standard library only)
- Creates: state machine with valid transitions (UNBOUND → BOUND_* → UNBOUND)
- Prevents: double-bind, unbind-without-bind
- Tests: state transitions, invalid transitions
- **Duration**: 1 hour
- **Files**:
  - Create: `SMPPServer/include/smpp_session.hpp`
  - Create: `SMPPServer/src/smpp_session.cpp`
  - Tests: `tests/unit/test_smpp_session.cpp`

**2. SmppProtocolAdapter** (protocol_layer.md)
- Dependencies: smppcxx (external library)
- Creates: bridge between TCP streaming and smppcxx typed objects
- Owns: PDU buffer, streaming state machine (WAITING_HEADER → WAITING_BODY → PDU_COMPLETE)
- Methods: process_tcp_data() → vector<Smpp::Header*>, encode_response() → bytes
- Tests: chunked input, multiple PDUs, all command types
- **Duration**: 2-3 hours
- **Files**:
  - Create: `SMPPServer/include/smpp_protocol_adapter.hpp`
  - Create: `SMPPServer/src/smpp_protocol_adapter.cpp`
  - Tests: `tests/unit/test_smpp_protocol_adapter.cpp`

---

### Phase 1C: Networking Foundation
ASIO async I/O infrastructure. Builds on SmppProtocolAdapter.

**3. SmppConnection** (presentation_layer.md)
- Dependencies: smppcxx (via SmppProtocolAdapter), SmppSession, SmppProtocolAdapter, SmppMessageProcessor
- Creates: per-client async state machine (ASIO async_read chain)
- Owns: socket, header_buffer, body_buffer
- Async callbacks: on_header_read(), on_body_read(), on_write()
- Tests: receive header → body → parse smppcxx objects → dispatch → send response
- **Duration**: 3-4 hours (complex ASIO async callback chain)
- **Files**:
  - Create: `SMPPServer/include/smpp_connection.hpp`
  - Create: `SMPPServer/src/smpp_connection.cpp`
  - Tests: `tests/integration/test_smpp_connection.cpp`

**4. TcpServer** (presentation_layer.md)
- Dependencies: SmppConnection, IpValidator, SmppMessageProcessor, SessionManager
- Creates: ASIO acceptor on port 2775
- Owns: acceptor_, active_connections_ set
- Async callback: on_accept()
- Tests: accept connections, validate IPs, create SmppConnection
- **Duration**: 2-3 hours
- **Files**:
  - Create: `SMPPServer/include/tcp_server.hpp`
  - Create: `SMPPServer/src/tcp_server.cpp`
  - Tests: `tests/integration/test_tcp_server.cpp`

---

### Phase 1D: Business Logic & Handlers
Message processing and command handling. Builds on SmppProtocolAdapter and SmppSession.

**5. BindTransmitterHandler** (business_logic_layer.md)
- Dependencies: smppcxx (Smpp::BindTransmitter, Smpp::BindTransmitterResp), DBusAuthenticator, Logger
- Creates: handler for BIND_TRANSMITTER command
- Logic: extract credentials from smppcxx object, validate via D-Bus, set state, build smppcxx response
- Tests: valid/invalid credentials, already bound
- **Duration**: 1.5 hours
- **Files**:
  - Create: `SMPPServer/include/handlers/bind_transmitter_handler.hpp`
  - Create: `SMPPServer/src/handlers/bind_transmitter_handler.cpp`
  - Tests: `tests/unit/test_bind_transmitter_handler.cpp`

**6. BindReceiverHandler** (business_logic_layer.md)
- Similar to BindTransmitterHandler, but Smpp::BindReceiver/BindReceiverResp and BOUND_RECEIVER state
- **Duration**: 1 hour
- **Files**:
  - Create: `SMPPServer/include/handlers/bind_receiver_handler.hpp`
  - Create: `SMPPServer/src/handlers/bind_receiver_handler.cpp`
  - Tests: `tests/unit/test_bind_receiver_handler.cpp`

**7. BindTransceiverHandler** (business_logic_layer.md)
- Similar to BindTransmitterHandler, but Smpp::BindTransceiver/BindTransceiverResp and BOUND_TRANSCEIVER state
- **Duration**: 1 hour
- **Files**:
  - Create: `SMPPServer/include/handlers/bind_transceiver_handler.hpp`
  - Create: `SMPPServer/src/handlers/bind_transceiver_handler.cpp`
  - Tests: `tests/unit/test_bind_transceiver_handler.cpp`

**8. UnbindHandler** (business_logic_layer.md)
- Handler for UNBIND: validate BOUND, call try_unbind(), return Smpp::UnbindResp
- **Duration**: 45 minutes
- **Files**:
  - Create: `SMPPServer/include/handlers/unbind_handler.hpp`
  - Create: `SMPPServer/src/handlers/unbind_handler.cpp`
  - Tests: `tests/unit/test_unbind_handler.cpp`

**9. EnquireLinkHandler** (business_logic_layer.md)
- Handler for ENQUIRE_LINK: validate BOUND, return Smpp::EnquireLinkResp
- **Duration**: 45 minutes
- **Files**:
  - Create: `SMPPServer/include/handlers/enquire_link_handler.hpp`
  - Create: `SMPPServer/src/handlers/enquire_link_handler.cpp`
  - Tests: `tests/unit/test_enquire_link_handler.cpp`

**10. SmppMessageProcessor** (business_logic_layer.md)
- Dependencies: All handlers, smppcxx (for polymorphic Header*), CredentialValidator, DBusAuthenticator, Logger
- Creates: router that type-checks smppcxx::Header* via dynamic_cast
- Methods: process_pdu(Smpp::Header*, session) → Smpp::Header* response
- Tests: route to correct handler, exception handling, unknown commands
- **Duration**: 1.5 hours
- **Files**:
  - Create: `SMPPServer/include/smpp_message_processor.hpp`
  - Create: `SMPPServer/src/smpp_message_processor.cpp`
  - Tests: `tests/unit/test_smpp_message_processor.cpp`

---

### Phase 1E: Infrastructure & Support Services
Utilities that can be done in parallel with Phase 1D.

**11. Logger** (infrastructure_layer.md)
- Dependencies: spdlog (via CMakeLists.txt FetchContent)
- Creates: spdlog wrapper with dual sinks (stdout + file)
- Static methods: init(), shutdown(), level_from_string()
- Macros: LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL
- Tests: init creates sinks, levels work, format is correct
- **Duration**: 1.5 hours
- **Files**:
  - Create: `SMPPServer/include/logger.hpp`
  - Create: `SMPPServer/src/logger.cpp`
  - Tests: `tests/unit/test_logger.cpp`

**12. IpValidator** (infrastructure_layer.md)
- Dependencies: None
- Creates: whitelist checker (CIDR support optional for v1)
- Methods: load_whitelist(path), is_whitelisted(ip)
- Tests: load file, exact match, CIDR match (if implemented)
- **Duration**: 1 hour
- **Files**:
  - Create: `SMPPServer/include/validators/ip_validator.hpp`
  - Create: `SMPPServer/src/validators/ip_validator.cpp`
  - Tests: `tests/unit/test_ip_validator.cpp`

**13. CredentialValidator** (infrastructure_layer.md)
- Dependencies: None
- Creates: format checker for username/password (used by handlers)
- Methods: validate_credentials(), extract_from_bind_body()
- Tests: valid/invalid formats, C-string extraction from binary
- **Duration**: 1 hour
- **Files**:
  - Create: `SMPPServer/include/validators/credential_validator.hpp`
  - Create: `SMPPServer/src/validators/credential_validator.cpp`
  - Tests: `tests/unit/test_credential_validator.cpp`

**14. DBusAuthenticator** (infrastructure_layer.md)
- Dependencies: sdbus-c++
- Creates: D-Bus bridge to SMPPAuthenticator service (used by BIND handlers)
- Methods: authenticate(username, password, client_ip)
- Tests: D-Bus method calls (mock if service not available)
- **Duration**: 2 hours (tricky D-Bus setup)
- **Files**:
  - Create: `SMPPServer/include/authenticators/dbus_authenticator.hpp`
  - Create: `SMPPServer/src/authenticators/dbus_authenticator.cpp`
  - Tests: `tests/unit/test_dbus_authenticator.cpp`

**15. SessionManager** (infrastructure_layer.md)
- Dependencies: SmppSession
- Creates: session registry (map session_id → SmppSession)
- Methods: create_session(), get_session(), remove_session(), cleanup_idle_sessions()
- Tests: create/retrieve/remove sessions, thread safety
- **Duration**: 1 hour
- **Files**:
  - Create: `SMPPServer/include/session_manager.hpp`
  - Create: `SMPPServer/src/session_manager.cpp`
  - Tests: `tests/unit/test_session_manager.cpp`

---

### Phase 2: Server Orchestrator
Brings everything together.

**16. SmppServer** (presentation_layer.md)
- Dependencies: All of above (DI container)
- Creates: orchestrator, starts TcpServer, runs io_context
- Methods: start(), stop(), run()
- Tests: starts cleanly, stops cleanly, accepts connections end-to-end
- **Duration**: 2 hours
- **Files**:
  - Create: `SMPPServer/include/smpp_server.hpp`
  - Create: `SMPPServer/src/smpp_server.cpp`
  - Modify: `SMPPServer/src/main.cpp` (arg parsing, logger init, SmppServer usage)
  - Tests: `tests/integration/test_smpp_server.cpp`, `tests/e2e/test_bind_flow.py`

---

## Per-Class Checklist

Use this template for each class:

### [ ] Class Name

**Phase**: 1A / 1B / 1C / 1D / 1E / 2

**Header file created**: `SMPPServer/include/[path/]class_name.hpp`
- [ ] Include guards
- [ ] Includes (system, then project)
- [ ] Forward declarations
- [ ] Class declaration with public/private sections
- [ ] Doxygen comments on public methods

**Source file created**: `SMPPServer/src/[path/]class_name.cpp`
- [ ] Includes
- [ ] Constructor implementation
- [ ] All public methods implemented
- [ ] All private helper methods implemented
- [ ] Member variable initialization in constructor
- [ ] Proper error handling (try-catch if needed)

**Tests created**: `tests/[unit|integration]/test_class_name.cpp`
- [ ] Test fixture (setUp/tearDown if needed)
- [ ] Happy path tests
- [ ] Edge case tests
- [ ] Error case tests
- [ ] All public methods covered

**CMakeLists.txt updated**:
- [ ] Add .hpp to `SMPPServer_HEADERS`
- [ ] Add .cpp to `SMPPServer_SOURCES`
- [ ] Add test .cpp as new test executable (if unit test)

**Documentation**:
- [ ] Class spec reviewed from class_specifications/*.md
- [ ] Implementation matches spec
- [ ] Design patterns applied correctly
- [ ] Dependencies injected via constructor

---

## Overall Timeline

| Phase | Classes | Est. Hours | Cumulative |
|---|---|---|---|
| 1A | SmppSession, SmppProtocolAdapter | 3-4 | 3-4 |
| 1C | SmppConnection, TcpServer | 5-7 | 8-11 |
| 1D | Handlers (5), SmppMessageProcessor | 8-9 | 16-20 |
| 1E | Logger, Validators (2), DBusAuthenticator, SessionManager | 6 | 22-26 |
| 2 | SmppServer, main.cpp | 2 | 24-28 |
| **Total** | **16 classes** | **~28 hours** | - |
| | (vs custom protocol: 32 hours) | **-4 hours** | (using smppcxx) |

---

## Verification Milestones

After each phase, verify:

**After Phase 1A**:
- SmppMessage encode/decode works
- SmppSession state transitions work

**After Phase 1B**:
- Parser handles streaming input
- Encoder builds correct responses

**After Phase 1C**:
- SmppConnection reads one PDU successfully
- TcpServer accepts a connection

**After Phase 1D**:
- Handlers process requests correctly
- SmppMessageProcessor routes correctly

**After Phase 1E**:
- Logger outputs to both sinks
- Validators work
- SessionManager tracks sessions

**After Phase 2**:
- Full end-to-end BIND flow: Client → Server accepts → BIND_TRANSMITTER request → Handler validates → Response → Client receives BIND_RESP_OK
- Run full test suite: `python3 tests/quick_test.py`

---

**Next**: header_file_templates.md
