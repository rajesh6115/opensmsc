# SMPPServer Low Level Design Overview

**Date**: 2026-04-19  
**Version**: 1.0  
**Status**: LLD Phase - Detailed Implementation Ready  

---

## Part 1: From HLD to LLD

### High Level Design (What We're Building)
```
HLD = What are the components? What do they do? How do they interact?

SYSTEM_OOAD_DESIGN.md    → "We need SMPPServer, SMPPAuthenticator, D-Bus, etc."
SMPPServer_HLD.md        → "We need 4 layers: Presentation, Protocol, Logic, Infrastructure"
CLASS_DESIGN_PLAN.md     → "We need 18 classes distributed across 4 layers"
SEQUENCE_DIAGRAMS.md     → "Here's how they work together (BIND flow, etc.)"
```

### Low Level Design (How to Build It)
```
LLD = What does each class look like? How do you code it? How do you test it?

LLD_Overview.md          → "Here's the implementation strategy and coding standards"
class_specifications/    → "Here's the detailed spec for each class"
implementation_guides/   → "Here's how to implement this class step-by-step"
testing/                 → "Here's how to test this class thoroughly"
error_handling/          → "Here's what errors can happen and how to handle them"
```

---

## Part 2: LLD Implementation Strategy

### 2.1 Phased Implementation Approach

#### Phase 1A: Foundation (Value Objects & Utilities)
These are built first because other classes depend on them.

```
Week 1:
├─ SmppMessage (protocol_layer)
│  └─ Immutable value object, no dependencies
│
├─ SmppSession (business_logic_layer)
│  └─ State machine, no external dependencies
│
├─ Enums & Constants (utilities)
│  └─ Command IDs, Status Codes, Error Codes
│
└─ Logger wrapper (infrastructure_layer)
   └─ spdlog wrapper (already exists, may refactor)
```

**Why First**: Other classes depend on SmppMessage and SmppSession. Build these solid.

---

#### Phase 1B: Protocol Handling
```
Week 2:
├─ SmppMessageParser (protocol_layer)
│  ├─ Depends on: SmppMessage
│  ├─ Responsible for: Bytes → SmppMessage
│  └─ Testing: Unit test with raw PDU bytes
│
└─ SmppMessageEncoder (protocol_layer)
   ├─ Depends on: SmppMessage
   ├─ Responsible for: SmppMessage → Bytes
   └─ Testing: Unit test encoding/decoding
```

**Why Second**: Now we can parse/encode messages. Needed for network layer.

---

#### Phase 1C: Networking Layer
```
Week 3:
├─ TcpServer (presentation_layer)
│  ├─ Depends on: ASIO, IpValidator
│  ├─ Responsible for: Socket management
│  └─ Testing: Integration test with raw sockets
│
├─ SmppConnection (presentation_layer)
│  ├─ Depends on: SmppMessageParser, SmppMessageProcessor
│  ├─ Responsible for: Per-client message handling
│  └─ Testing: Integration test with mocked processor
│
└─ SmppServer (presentation_layer)
   ├─ Depends on: TcpServer, all validators/authenticators
   ├─ Responsible for: Service lifecycle
   └─ Testing: Integration test full startup/shutdown
```

**Why Third**: Socket handling after message protocol is defined.

---

#### Phase 1D: Business Logic
```
Week 4:
├─ SmppMessageProcessor (business_logic_layer)
│  ├─ Depends on: SmppMessage, SmppSession, Handlers
│  ├─ Responsible for: Message → Handler routing
│  └─ Testing: Unit test routing logic
│
└─ 5× Handlers (business_logic_layer)
   ├─ BindTransmitterHandler
   ├─ BindReceiverHandler
   ├─ BindTransceiverHandler
   ├─ UnbindHandler
   └─ EnquireLinkHandler
   ├─ Depends on: SmppMessage, SmppSession, validators
   ├─ Responsible for: Command-specific logic
   └─ Testing: Unit test each handler independently
```

**Why Fourth**: Handlers use Session, Validators, etc. Build those first.

---

#### Phase 1E: Infrastructure (Parallel)
```
Throughout:
├─ IpValidator (infrastructure_layer)
│  ├─ Depends on: Config files
│  └─ Testing: Unit test whitelist logic
│
├─ CredentialValidator (infrastructure_layer)
│  ├─ Depends on: Config files
│  └─ Testing: Unit test password hashing
│
├─ DBusAuthenticator (infrastructure_layer)
│  ├─ Depends on: D-Bus, sdbus-c++
│  └─ Testing: Integration test D-Bus calls
│
└─ SessionManager (infrastructure_layer)
   ├─ Depends on: SmppSession
   └─ Testing: Unit test session tracking
```

**Why Parallel**: These can be built alongside other layers.

---

### 2.2 Implementation Order Summary

```
CRITICAL PATH (must build first):
1. SmppMessage
2. SmppSession
3. SmppMessageParser
4. SmppMessageEncoder
5. SmppConnection
6. SmppMessageProcessor
7. Handlers (5×)
8. SmppServer

SUPPORTING (can build in parallel):
- IpValidator
- CredentialValidator
- DBusAuthenticator
- SessionManager
- Logger (already exists)
```

---

## Part 3: Coding Standards & Patterns

### 3.1 Class Structure Template

```cpp
// File: include/my_class.hpp
#pragma once

#include <memory>
#include <string>
// ... other includes

// Forward declarations (avoid circular dependencies)
class SmppSession;
class Logger;

/**
 * @brief One-line description of class
 * 
 * Longer description explaining:
 * - What this class is responsible for
 * - What it depends on
 * - What depends on it
 * 
 * @example
 * auto obj = MyClass(dependency1, dependency2);
 * obj.do_something();
 */
class MyClass {
public:
    /**
     * @param dep1 Description
     * @param dep2 Description
     */
    MyClass(
        std::shared_ptr<Dependency1> dep1,
        std::shared_ptr<Dependency2> dep2);
    
    // Public interface (minimal, only what's needed)
    void public_method();
    std::string query_state() const;
    
private:
    // Private implementation
    void private_helper();
    
    // Members (const for immutables, mutable for state)
    std::shared_ptr<Dependency1> dep1_;
    std::shared_ptr<Dependency2> dep2_;
    std::string state_;
};
```

### 3.2 Constructor Dependency Injection

```cpp
// GOOD: Dependencies injected, easy to test
class MyClass {
public:
    MyClass(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Validator> validator)
        : logger_(logger), validator_(validator) {}
};

// BAD: Dependencies created internally, hard to test
class MyClass {
public:
    MyClass() {
        logger_ = std::make_shared<Logger>();  // Can't mock!
        validator_ = std::make_shared<Validator>();  // Can't swap!
    }
};
```

### 3.3 Const Correctness

```cpp
class SmppSession {
public:
    // Query methods are const (don't modify state)
    State state() const { return state_; }
    bool is_bound() const { return state_ == State::BOUND_TX; }
    
    // Mutation methods are non-const
    bool try_bind_as_transmitter(const std::string& user);
    void touch();  // Updates last_activity
};
```

### 3.4 Error Handling Strategy

```cpp
// Option 1: Return false for expected errors
bool DBusAuthenticator::authenticate(
    const std::string& user,
    const std::string& pass) const {
    // D-Bus call failed → return false (expected, not exceptional)
    return false;
}

// Option 2: Exception for unexpected errors
SmppMessage SmppMessageEncoder::build_response(...) {
    if (status > 0xFFFFFFFF) {
        throw std::invalid_argument("Status code out of range");
    }
}

// Option 3: Logging + status code (for SMPP errors)
SmppMessage SmppMessageProcessor::process_message(...) {
    if (!session.is_bound()) {
        logger_->warn("ENQUIRE_LINK from unbound session");
        return SmppMessageEncoder::build_error_response(
            0x0D,  // ESME_RBINDFAIL
            seq_num);
    }
}
```

### 3.5 Logging Pattern

```cpp
// Log at decision points
class BindHandler {
    SmppMessage handle(...) {
        logger_->info("BIND_TRANSMITTER from {}", user);
        
        if (!validator->validate(user, pass)) {
            logger_->warn("Auth failed for user {}", user);
            return error_response(0x0E);  // ESME_RINVPASWD
        }
        
        if (!session.try_bind_as_transmitter(user)) {
            logger_->warn("State transition failed, current state = {}", session.state());
            return error_response(0x05);  // ESME_RALYBND
        }
        
        logger_->info("Successfully bound user {} as transmitter", user);
        return success_response();
    }
};
```

### 3.6 Thread Safety

```cpp
// If shared between threads, use mutex
class SessionManager {
private:
    std::map<std::string, std::shared_ptr<SmppSession>> sessions_;
    mutable std::mutex lock_;
    
public:
    std::shared_ptr<SmppSession> get_session(const std::string& id) const {
        std::lock_guard<std::mutex> guard(lock_);
        auto it = sessions_.find(id);
        return it != sessions_.end() ? it->second : nullptr;
    }
};

// If single-threaded (per connection), no lock needed
class SmppConnection {
    // No mutex - only accessed by ASIO callback for this socket
    std::shared_ptr<SmppSession> session_;
};
```

---

## Part 4: Testing Strategy

### 4.1 Test Pyramid (for SMPPServer)

```
                    ╱╲
                   ╱  ╲         E2E Tests (10%)
                  ╱────╲        Full platform test
                 ╱      ╲       With real D-Bus, real sockets
                ╱════════╲
               ╱          ╲     Integration Tests (25%)
              ╱  Handlers  ╲    Class interaction tests
             ╱  with deps   ╲   Mocked external services
            ╱════════════════╲
           ╱                  ╲  Unit Tests (65%)
          ╱   Pure logic,     ╲  Isolated classes
         ╱    value objects   ╲  Dependency injection
        ╱════════════════════════╲
```

### 4.2 Unit Testing Approach

```cpp
// Test file: tests/unit/test_smpp_message.cpp

#include <gtest/gtest.h>
#include "smpp_message.hpp"

class SmppMessageTest : public ::testing::Test {
protected:
    // No setup needed - SmppMessage has no dependencies
};

TEST_F(SmppMessageTest, ConstructorInitializesFields) {
    SmppMessage msg(0x00000002, 0, 1, {});
    
    EXPECT_EQ(msg.command_id(), 0x00000002);
    EXPECT_EQ(msg.command_status(), 0);
    EXPECT_EQ(msg.sequence_number(), 1);
}

TEST_F(SmppMessageTest, EncodingProducesValidPdu) {
    SmppMessage msg(0x00000002, 0, 1, {});
    auto bytes = msg.encode();
    
    EXPECT_GE(bytes.size(), 16);  // At least header
    EXPECT_EQ(bytes[4], 0x00);    // First byte of command_id
}

TEST_F(SmppMessageTest, DecodingRoundtrip) {
    SmppMessage original(0x00000002, 0, 1, {});
    auto bytes = original.encode();
    
    auto decoded = SmppMessage::decode(bytes);
    
    EXPECT_EQ(decoded.command_id(), original.command_id());
    EXPECT_EQ(decoded.sequence_number(), original.sequence_number());
}
```

### 4.3 Integration Testing Approach

```cpp
// Test file: tests/integration/test_bind_handler.cpp

#include <gtest/gtest.h>
#include "bind_transmitter_handler.hpp"
#include "mock_authenticator.hpp"  // Mock D-Bus

class BindHandlerTest : public ::testing::Test {
protected:
    std::shared_ptr<MockDBusAuthenticator> mock_auth_;
    std::shared_ptr<Logger> logger_;
    std::unique_ptr<BindTransmitterHandler> handler_;
    SmppSession session_{"session1", "127.0.0.1"};
    
    void SetUp() override {
        mock_auth_ = std::make_shared<MockDBusAuthenticator>();
        logger_ = std::make_shared<Logger>();
        handler_ = std::make_unique<BindTransmitterHandler>(mock_auth_, logger_);
    }
};

TEST_F(BindHandlerTest, SuccessfulAuthentication) {
    // Setup mock
    EXPECT_CALL(*mock_auth_, authenticate("test", "test"))
        .WillOnce(::testing::Return(true));
    
    // Create BIND message
    SmppMessage bind_msg = create_bind_transmitter_pdu("test", "test", 1);
    
    // Handle
    auto response = handler_->handle(bind_msg, session_);
    
    // Verify
    EXPECT_EQ(response.command_status(), 0);  // Success
    EXPECT_TRUE(session_.is_bound());
    EXPECT_EQ(session_.authenticated_as(), "test");
}

TEST_F(BindHandlerTest, FailedAuthentication) {
    // Setup mock
    EXPECT_CALL(*mock_auth_, authenticate("test", "wrong"))
        .WillOnce(::testing::Return(false));
    
    // Handle
    SmppMessage bind_msg = create_bind_transmitter_pdu("test", "wrong", 1);
    auto response = handler_->handle(bind_msg, session_);
    
    // Verify
    EXPECT_EQ(response.command_status(), 0x0E);  // ESME_RINVPASWD
    EXPECT_FALSE(session_.is_bound());
}
```

---

## Part 5: Development Checklist

### For Each Class Implementation:

- [ ] Create `.hpp` header file (use template from implementation_guides)
- [ ] Write class specification doc (see class_specifications/)
- [ ] Implement `.cpp` source file
- [ ] Follow coding standards from Part 3 above
- [ ] Add constructor with dependency injection
- [ ] Add public interface (minimal, only needed methods)
- [ ] Add error handling (use strategies from Part 3)
- [ ] Add logging at decision points
- [ ] Write unit tests (no external dependencies)
- [ ] Write integration tests (with mocked dependencies)
- [ ] Update CMakeLists.txt with new files
- [ ] Run tests locally before committing
- [ ] Update this checklist progress

---

## Part 6: Key Design Decisions

### Decision 1: Value Object Pattern for SmppMessage

**Question**: Should SmppMessage be mutable or immutable?

**Decision**: **Immutable value object**

**Rationale**:
- Messages don't change after creation
- Easier to reason about (no hidden state changes)
- Safe to pass around without locks
- Can be safely cached

```cpp
class SmppMessage {
    // No setters - immutable
    uint32_t command_id() const { return command_id_; }
    
    // Created via constructor or decode()
    SmppMessage(uint32_t cmd_id, uint32_t status, uint32_t seq, ...);
};
```

---

### Decision 2: State Machine for SmppSession

**Question**: How to prevent invalid state transitions?

**Decision**: **Methods that validate before changing state**

**Rationale**:
- Prevents double-bind, unbind-without-bind, etc.
- Returns bool (false if invalid transition)
- Clear, explicit state model

```cpp
class SmppSession {
    // Can't accidentally go UNBOUND → BOUND_TX → BOUND_RX
    bool try_bind_as_transmitter(...);  // UNBOUND → BOUND_TX
    bool try_bind_as_receiver(...);     // UNBOUND → BOUND_RX
    bool try_unbind();                  // BOUND_* → UNBOUND
};
```

---

### Decision 3: Async I/O with ASIO

**Question**: Threading model - how many threads?

**Decision**: **Single-threaded event loop per acceptor**

**Rationale**:
- ASIO handles thousands of connections with one thread
- No context switching overhead
- Simpler to reason about (no locks except for shared state)

```cpp
// In main.cpp
asio::io_context io_ctx;
TcpServer server(io_ctx, 2775, ...);

server.start();
io_ctx.run();  // Blocks until all work done (or stop() called)
```

---

### Decision 4: D-Bus for Authentication

**Question**: Why D-Bus instead of direct function call?

**Decision**: **D-Bus for service modularity**

**Rationale**:
- SMPPAuthenticator is separate service
- Can restart authenticator without restarting SMPP server
- Can replace authenticator implementation (local vs. LDAP vs. DB)
- Follows Unix philosophy (one service, one job)

---

### Decision 5: Error Codes vs. Exceptions

**Question**: When to use exceptions vs. return values?

**Decision**:
- **Return false**: Expected errors (auth failure, state violation)
- **Exception**: Programming errors (invalid arg, out of range)
- **SMPP Error Response**: Business logic errors

**Rationale**:
- Exceptions for bugs (caught in tests)
- Return values for expected failures (auth, state)
- SMPP codes for client errors (need to respond to them)

---

## Part 7: Common Pitfalls & How to Avoid

### Pitfall 1: Circular Dependencies

```cpp
// BAD: smpp_connection.h includes smpp_processor.h
// smpp_processor.h includes smpp_connection.h
// → Compiler error!

// GOOD: Use forward declaration
// smpp_connection.h
class SmppMessageProcessor;  // Forward declare

class SmppConnection {
    std::shared_ptr<SmppMessageProcessor> processor_;  // OK
};
```

### Pitfall 2: Missing Error Handling

```cpp
// BAD: Assumes socket write always succeeds
socket.write(data);  // What if socket closed?

// GOOD: Check error code
asio::error_code ec;
socket.write(asio::buffer(data), ec);
if (ec) {
    logger_->error("Socket write failed: {}", ec.message());
    close_connection();
}
```

### Pitfall 3: Shared State Without Locks

```cpp
// BAD: SessionManager used from multiple ASIO callbacks
std::map<std::string, SmppSession> sessions_;  // Race condition!

// GOOD: Protect with mutex
std::map<std::string, SmppSession> sessions_;
mutable std::mutex sessions_lock_;
```

### Pitfall 4: Logging Everything

```cpp
// BAD: Logs at info level for every single byte
logger_->info("Received byte: 0x{:02x}", byte);  // Spam!

// GOOD: Log at decision points only
logger_->info("BIND_TRANSMITTER from {}", user);
logger_->debug("Parsed PDU: size={}, type=0x{:08x}", size, type);
```

### Pitfall 5: Tight Coupling to D-Bus

```cpp
// BAD: SmppHandler directly calls D-Bus
class BindHandler {
    void handle(...) {
        // Calls D-Bus directly - can't test without D-Bus running
        auto result = dbus_authenticate(user, pass);
    }
};

// GOOD: DBusAuthenticator abstraction
class BindHandler {
    BindHandler(std::shared_ptr<DBusAuthenticator> auth) : auth_(auth) {}
    
    void handle(...) {
        // Calls abstraction - can inject mock for testing
        auto result = auth_->authenticate(user, pass);
    }
};
```

---

## Part 8: Quick Reference

### Build Command
```bash
cd /workspace
rm -rf build && mkdir build && cd build
cmake ..
cmake --build . --parallel 4
```

### Running Tests
```bash
ctest --output-on-failure
```

### Adding a New Class

1. Create `include/my_class.hpp`
2. Create `src/my_class.cpp`
3. Update `CMakeLists.txt`
4. Write unit tests in `tests/unit/test_my_class.cpp`
5. Write integration tests in `tests/integration/test_my_class.cpp`
6. Run `cmake --build . && ctest`
7. Create class spec doc in `doc/SmppServer/class_specifications/`

---

## Part 9: Related Documents

| Document | Purpose | Read When |
|---|---|---|
| README.md | Navigation guide | Starting implementation |
| CLASS_DESIGN_PLAN.md | All 18 classes with responsibilities | Need class overview |
| class_specifications/*.md | Detailed spec for one class | Implementing that class |
| implementation_guides/*.md | How to code this class | Writing the .hpp/.cpp |
| testing/*.md | How to test this class | Writing tests |
| SEQUENCE_DIAGRAMS.md | How classes work together | Understanding flows |

---

## Conclusion

**LLD Strategy**:
1. **Build in phases** - Foundation → Protocol → Network → Business Logic
2. **Inject dependencies** - Easy to test, easy to swap implementations
3. **Test thoroughly** - Unit + Integration tests per class
4. **Log strategically** - At decision points, not in loops
5. **Handle errors explicitly** - No silent failures
6. **Document as you code** - Class specs and test cases ARE documentation

**Ready to start?**
→ Pick first class from Phase 1A (SmppMessage)
→ Read detailed spec in `class_specifications/protocol_layer.md`
→ Use template from `implementation_guides/header_file_templates.md`
→ Follow this LLD for coding standards

---

**Document Status**: Complete - Ready for Implementation  
**Next Steps**:
1. ✓ Review this LLD_Overview.md
2. → Start with `class_specifications/protocol_layer.md` (SmppMessage)
3. → Use `implementation_guides/` for coding templates
4. → Run tests after each class

**Owner**: Development Team  
**Last Updated**: 2026-04-19  
