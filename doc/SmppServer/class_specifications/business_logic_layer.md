# Business Logic Layer: Class Specifications

**Date**: 2026-04-19  
**Version**: 1.0  
**Layer**: Business Logic Layer (Application Logic)  
**Classes**: SmppSession, SmppMessageProcessor, SmppRequestHandler, 5× Handlers  

---

## Overview

The Business Logic Layer implements SMPP protocol state machine and command handling.

- **SmppSession**: State machine (UNBOUND → BOUND → UNBOUND)
- **SmppMessageProcessor**: Routes messages to appropriate handlers
- **SmppRequestHandler**: Interface for all command handlers
- **5× Concrete Handlers**: BindTransmitter, BindReceiver, BindTransceiver, Unbind, EnquireLink

---

## Class 1: SmppSession

### Purpose
Model the state of one SMPP connection. Enforce valid state transitions.

### Location
- **Header**: `SMPPServer/include/smpp_session.hpp`
- **Source**: `SMPPServer/src/smpp_session.cpp`

### Responsibilities

**State Management**:
- Track connection state (UNBOUND → BOUND_TX/RX/TRX → UNBOUND)
- Store authenticated username
- Track connection metadata (IP, session ID, timestamps)

**State Transition Validation**:
- Prevent invalid transitions (double-bind, unbind-without-bind)
- Return bool (false if transition invalid)

**Activity Tracking**:
- Track when connection established
- Track last activity time (for idle detection)

### Class Signature

```cpp
class SmppSession {
public:
    enum class State {
        UNBOUND,              // Initial state, not bound
        BINDING,              // Optional: during bind process (if async)
        BOUND_TRANSMITTER,    // Can transmit (client sends)
        BOUND_RECEIVER,       // Can receive (server sends)
        BOUND_TRANSCEIVER,    // Both directions
        UNBINDING,            // Optional: during unbind process
        CLOSED                // Connection closed
    };
    
    // Constructor
    SmppSession(
        const std::string& session_id,
        const std::string& client_ip);
    
    // Read-only accessors
    const std::string& session_id() const;
    const std::string& client_ip() const;
    State state() const;
    const std::string& authenticated_as() const;
    std::time_t connected_at() const;
    std::time_t last_activity() const;
    
    // State queries
    bool is_bound() const;
    bool can_transmit() const;  // BOUND_TX or BOUND_TRX
    bool can_receive() const;   // BOUND_RX or BOUND_TRX
    
    // State transitions (return false if invalid)
    bool try_bind_as_transmitter(const std::string& username);
    bool try_bind_as_receiver(const std::string& username);
    bool try_bind_as_transceiver(const std::string& username);
    bool try_unbind();
    bool try_close();
    
    // Activity tracking
    void touch();  // Update last_activity to now
    
private:
    std::string session_id_;
    std::string client_ip_;
    State state_;
    std::string authenticated_as_;
    std::time_t connected_at_;
    std::time_t last_activity_;
};
```

### State Machine Diagram

```
    NEW SESSION
        │
        ↓
    ┌─────────────────────┐
    │      UNBOUND        │  ← Initial state
    └─────────────────────┘
        │
        ├─ try_bind_as_transmitter("user")
        │  └─→ BOUND_TRANSMITTER
        │
        ├─ try_bind_as_receiver("user")
        │  └─→ BOUND_RECEIVER
        │
        └─ try_bind_as_transceiver("user")
           └─→ BOUND_TRANSCEIVER
    
    ┌──────────────────────────────────────┐
    │   BOUND_TRANSMITTER                  │
    │   (Can send, client → server)        │
    └──────────────────────────────────────┘
        │
        ├─ try_bind_as_receiver("user")
        │  └─→ Fails (already bound)
        │
        ├─ try_bind_as_transmitter("user")
        │  └─→ Fails (already bound)
        │
        └─ try_unbind()
           └─→ UNBOUND
    
    ┌──────────────────────────────────────┐
    │   BOUND_RECEIVER                     │
    │   (Can receive, server → client)     │
    └──────────────────────────────────────┘
        │
        └─ try_unbind() → UNBOUND
    
    ┌──────────────────────────────────────┐
    │   BOUND_TRANSCEIVER                  │
    │   (Both directions)                  │
    └──────────────────────────────────────┘
        │
        └─ try_unbind() → UNBOUND
    
    ┌─────────────────────┐
    │      UNBOUND        │  ← After UNBIND
    └─────────────────────┘
        │
        └─ try_close() → CLOSED → [connection cleanup]
```

### Key Implementation Details

**Timestamp Tracking**:
```cpp
SmppSession::SmppSession(
    const std::string& session_id,
    const std::string& client_ip)
    : session_id_(session_id),
      client_ip_(client_ip),
      state_(State::UNBOUND),
      authenticated_as_(""),
      connected_at_(std::time(nullptr)),
      last_activity_(std::time(nullptr)) {}
```

**State Transition Validation**:
```cpp
bool SmppSession::try_bind_as_transmitter(
    const std::string& username) {
    
    // Only allow from UNBOUND state
    if (state_ != State::UNBOUND) {
        return false;  // Already bound or invalid state
    }
    
    state_ = State::BOUND_TRANSMITTER;
    authenticated_as_ = username;
    touch();
    return true;
}
```

**Activity Tracking**:
```cpp
void SmppSession::touch() {
    last_activity_ = std::time(nullptr);
}

// Called in SmppConnection when any message processed
void SmppConnection::on_message_received() {
    session_->touch();
}
```

### Dependencies
- None (pure domain logic)

---

## Class 2: SmppMessageProcessor

### Purpose
Route incoming SMPP PDU objects (from smppcxx) to appropriate command handlers.

### Location
- **Header**: `SMPPServer/include/smpp_message_processor.hpp`
- **Source**: `SMPPServer/src/smpp_message_processor.cpp`

### Responsibilities

**PDU Routing**:
- Receive smppcxx::Header* (polymorphic SMPP object)
- Identify concrete type (BindReceiver, BindTransmitter, etc.)
- Route to appropriate handler
- Call handler with typed request object and session

**State Validation**:
- Check if session is in valid state for this command
- Return error response if invalid

**Error Handling**:
- Catch exceptions from handlers
- Build error responses via smppcxx
- Log issues

### Class Signature

```cpp
class SmppMessageProcessor {
public:
    SmppMessageProcessor(
        std::shared_ptr<CredentialValidator> cred_validator,
        std::shared_ptr<DBusAuthenticator> dbus_auth,
        std::shared_ptr<Logger> logger);
    
    // Main processing function (polymorphic smppcxx object)
    std::unique_ptr<Smpp::Header> process_pdu(
        std::unique_ptr<Smpp::Header> request,
        SmppSession& session);
    
private:
    // Handler methods per command type (smppcxx objects)
    Smpp::Header* handle_bind_transmitter(
        const Smpp::BindTransmitter& request,
        SmppSession& session);
    
    Smpp::Header* handle_bind_receiver(
        const Smpp::BindReceiver& request,
        SmppSession& session);
    
    Smpp::Header* handle_bind_transceiver(
        const Smpp::BindTransceiver& request,
        SmppSession& session);
    
    Smpp::Header* handle_unbind(
        const Smpp::Unbind& request,
        SmppSession& session);
    
    Smpp::Header* handle_enquire_link(
        const Smpp::EnquireLink& request,
        SmppSession& session);
    
    Smpp::Header* handle_unknown_command(
        Smpp::Uint32 command_id,
        Smpp::Uint32 sequence_number);
    
    std::shared_ptr<CredentialValidator> cred_validator_;
    std::shared_ptr<DBusAuthenticator> dbus_auth_;
    std::shared_ptr<Logger> logger_;
};
```

### Processing Flow

```
process_pdu(request, session)
    │
    ├─ request is polymorphic Smpp::Header*
    │
    ├─ Log incoming message
    │   logger_->info("BIND_TRANSMITTER seq={}", request->sequence_number())
    │
    ├─ Type-cast and route to handler
    │   ├─ if (auto* bind_tx = dynamic_cast<Smpp::BindTransmitter*>(...))
    │   │  └─ handle_bind_transmitter(*bind_tx, session)
    │   ├─ if (auto* bind_rx = dynamic_cast<Smpp::BindReceiver*>(...))
    │   │  └─ handle_bind_receiver(*bind_rx, session)
    │   ├─ if (auto* bind_trx = dynamic_cast<Smpp::BindTransceiver*>(...))
    │   │  └─ handle_bind_transceiver(*bind_trx, session)
    │   ├─ if (auto* unbind = dynamic_cast<Smpp::Unbind*>(...))
    │   │  └─ handle_unbind(*unbind, session)
    │   ├─ if (auto* enquire = dynamic_cast<Smpp::EnquireLink*>(...))
    │   │  └─ handle_enquire_link(*enquire, session)
    │   └─ else → handle_unknown_command()
    │
    ├─ Handler processes, returns smppcxx response object
    │
    ├─ Log result
    │   ├─ if success: logger_->info("BIND successful")
    │   └─ if error: logger_->warn("Auth failed")
    │
    └─ Return response (Smpp::Header*)
```

### Key Implementation Details

**Command Routing**:
```cpp
SmppMessage SmppMessageProcessor::process_message(
    const SmppMessage& request,
    SmppSession& session) {
    
    try {
        switch (request.command_id()) {
        case 0x00000001:  // BIND_RECEIVER
            return handle_bind_receiver(request, session);
            
        case 0x00000002:  // BIND_TRANSMITTER
            return handle_bind_transmitter(request, session);
            
        case 0x00000009:  // BIND_TRANSCEIVER
            return handle_bind_transceiver(request, session);
            
        case 0x00000006:  // UNBIND
            return handle_unbind(request, session);
            
        case 0x0000000F:  // ENQUIRE_LINK
            return handle_enquire_link(request, session);
            
        default:
            logger_->warn("Unknown command 0x{:08x}", request.command_id());
            return handle_unknown_command(
                request.command_id(),
                request.sequence_number());
        }
    } catch (const std::exception& ex) {
        logger_->error("Exception processing message: {}", ex.what());
        return SmppMessageEncoder::build_generic_resp(
            request.command_id(),
            0xFF,  // Generic error
            request.sequence_number());
    }
}
```

### Dependencies
- SmppMessage
- SmppSession
- CredentialValidator
- DBusAuthenticator
- Logger

---

## Class 3: Handlers (No Interface Needed)

### Purpose
Concrete handler functions for each SMPP command. No interface—handlers are direct functions that work with smppcxx types.

### Location
- **Headers**: `SMPPServer/include/handlers/*.hpp`
- **Sources**: `SMPPServer/src/handlers/*.cpp`

### Why No Interface?

With smppcxx providing typed objects, we don't need a common interface. Each handler has the signature:

```cpp
Smpp::Header* handle_<command>(
    const Smpp::<CommandType>& request,
    SmppSession& session);
```

The processor routes by type-checking the polymorphic `Smpp::Header*`:

```cpp
if (auto* req = dynamic_cast<Smpp::BindTransmitter*>(pdu.get())) {
    return handle_bind_transmitter(*req, session);
}
```

This is simpler than a virtual base class and leverages C++ RTTI.

---

## Class 4-8: Concrete Handlers

### Pattern: All Handlers Work with smppcxx Objects

```cpp
// Handlers work with smppcxx objects, not an interface
class BindTransmitterHandler {
public:
    BindTransmitterHandler(
        std::shared_ptr<DBusAuthenticator> auth,
        std::shared_ptr<Logger> logger);
    
    // Takes smppcxx::BindTransmitter (typed request)
    // Returns smppcxx::Header* (response, allocated on heap)
    Smpp::Header* handle(
        const Smpp::BindTransmitter& request,
        SmppSession& session);
    
private:
    std::shared_ptr<DBusAuthenticator> auth_;
    std::shared_ptr<Logger> logger_;
};
```

### Handler 1: BindTransmitterHandler

**Purpose**: Handle BIND_TRANSMITTER request (smppcxx::BindTransmitter)

**Location**:
- `SMPPServer/include/handlers/bind_transmitter_handler.hpp`
- `SMPPServer/src/handlers/bind_transmitter_handler.cpp`

**Logic**:
1. Check session is UNBOUND
2. Extract system_id and password from smppcxx object
3. Call DBusAuthenticator to validate
4. If valid:
   - Call session.try_bind_as_transmitter(username)
   - Create Smpp::BindTransmitterResp, set status 0 (ESME_ROK)
   - Return response
5. If invalid:
   - Create Smpp::BindTransmitterResp, set status 0x0000000E (ESME_RINVPASWD)
   - Return error response

**Key Points**:
- Input: `const Smpp::BindTransmitter& request` (from smppcxx)
- Output: `Smpp::Header*` (caller owns, typically BindTransmitterResp*)
- Extract fields: `request.system_id()`, `request.password()`, `request.sequence_number()`
- Create response: `new Smpp::BindTransmitterResp()`, then set fields

**Test Cases**:
- Valid credentials → success response with ESME_ROK
- Invalid credentials → error response with ESME_RINVPASWD
- Already bound → error response with ESME_RALYBND
- Invalid state → error response

### Handler 2: BindReceiverHandler

**Similar to BindTransmitterHandler** but:
- Input: `const Smpp::BindReceiver&`
- Output: `Smpp::BindReceiverResp*`
- Calls session.try_bind_as_receiver()
- Sets BOUND_RECEIVER state

### Handler 3: BindTransceiverHandler

**Similar to BindTransmitterHandler** but:
- Input: `const Smpp::BindTransceiver&`
- Output: `Smpp::BindTransceiverResp*`
- Calls session.try_bind_as_transceiver()
- Sets BOUND_TRANSCEIVER state

### Handler 4: UnbindHandler

**Purpose**: Handle UNBIND request (smppcxx::Unbind)

**Logic**:
1. Check session is bound
2. Call session.try_unbind()
3. Create Smpp::UnbindResp with status 0 (success)
4. Return response

**Test Cases**:
- From BOUND_TX → success with ESME_ROK
- From BOUND_RX → success with ESME_ROK
- From BOUND_TRX → success with ESME_ROK
- From UNBOUND → error response

### Handler 5: EnquireLinkHandler

**Purpose**: Handle ENQUIRE_LINK (keep-alive) (smppcxx::EnquireLink)

**Logic**:
1. Check session is bound
2. Create Smpp::EnquireLinkResp with status 0 (success)
3. Return immediate response
4. No state change

**Test Cases**:
- From bound → success response with ESME_ROK
- From unbound → error response

---

## Handler Patterns

All handlers:
- Are **stateless** (no member variables except dependencies)
- Can be **reused** across multiple sessions
- Are **thread-safe** (no modifications to shared state)
- **Inject dependencies** via constructor
- **Don't modify session directly** - use try_* methods that validate
- Return **owned pointers** (`Smpp::Header*`) that caller frees

```cpp
// Handler instance created once, shared
auto bind_handler = std::make_shared<BindTransmitterHandler>(auth, logger);

// Reused for every BIND_TRANSMITTER message from any client
auto* resp1 = bind_handler->handle(bind_tx_msg1, session1);  // Returns new BindTransmitterResp
auto* resp2 = bind_handler->handle(bind_tx_msg2, session2);  // Returns new BindTransmitterResp
// Caller responsible for deleting responses
```

---

## Dependencies

**SmppSession**:
- None (pure domain logic)

**SmppMessageProcessor**:
- smppcxx::Header (and derived classes: BindReceiver, BindTransmitter, etc.)
- SmppSession
- CredentialValidator
- DBusAuthenticator
- Logger

**Handlers**:
- smppcxx::Header, smppcxx::BindReceiver/Transmitter/Transceiver, smppcxx::Unbind, smppcxx::EnquireLink (input types)
- smppcxx::BindTransmitterResp, BindReceiverResp, etc. (response creation)
- SmppSession
- DBusAuthenticator (for BIND handlers)
- Logger

---

## Testing Checklist

- [ ] SmppSession state transitions
- [ ] SmppSession prevents invalid transitions
- [ ] SmppSession tracks last_activity
- [ ] SmppMessageProcessor routes to correct smppcxx type via dynamic_cast
- [ ] SmppMessageProcessor handles exceptions from handlers
- [ ] SmppMessageProcessor handles unknown command IDs
- [ ] BindTransmitterHandler succeeds with valid credentials (returns BindTransmitterResp with status 0)
- [ ] BindTransmitterHandler fails with invalid credentials (returns status 0x0000000E)
- [ ] BindTransmitterHandler prevents double-bind (returns status 0x00000005)
- [ ] UnbindHandler succeeds from all BOUND states (returns UnbindResp with status 0)
- [ ] UnbindHandler fails from UNBOUND state
- [ ] EnquireLinkHandler succeeds only when bound (returns EnquireLinkResp with status 0)
- [ ] EnquireLinkHandler fails when not bound
- [ ] All handlers return correct smppcxx response objects with correct status codes

---

**Document Status**: Complete  
**Next**: presentation_layer.md  
