# Unit Testing Strategy

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: How to write unit tests for each class  

---

## Testing Framework: Catch2

We use Catch2 for unit tests (header-only, easy to integrate via CMakeLists.txt FetchContent).

### Setup

```cmake
# In SMPPServer/CMakeLists.txt
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0)
FetchContent_MakeAvailable(Catch2)

# Link test executables
target_link_libraries(test_smpp_message Catch2::Catch2WithMain smpp_server_lib)
```

### Test File Organization

```
tests/
├── unit/
│   ├── test_smpp_message.cpp
│   ├── test_smpp_session.cpp
│   ├── test_smpp_message_parser.cpp
│   ├── test_smpp_message_encoder.cpp
│   ├── test_ip_validator.cpp
│   ├── test_credential_validator.cpp
│   ├── test_dbus_authenticator.cpp
│   ├── test_logger.cpp
│   ├── test_session_manager.cpp
│   ├── test_bind_transmitter_handler.cpp
│   ├── test_unbind_handler.cpp
│   └── test_enquire_link_handler.cpp
├── integration/
│   ├── test_smpp_connection.cpp
│   ├── test_tcp_server.cpp
│   └── test_smpp_server.cpp
├── e2e/
│   └── test_bind_flow.py
└── mocks/
    ├── mock_dbus_authenticator.hpp
    ├── mock_logger.hpp
    └── mock_socket.hpp
```

---

## Unit Test Template

```cpp
#include <catch2/catch_test_macros.hpp>
#include "class_name.hpp"
#include "mock_dependency.hpp"

namespace smpp {

TEST_CASE("ClassName", "[class_name]") {
    // Arrange: Setup test fixtures and mocks
    auto mock_dep = std::make_shared<MockDependency>();
    auto obj = std::make_shared<ClassName>(mock_dep);
    
    SECTION("Happy path - method succeeds") {
        // Arrange
        // ... setup specific to this section
        
        // Act
        auto result = obj->method(param);
        
        // Assert
        REQUIRE(result == expected);
    }
    
    SECTION("Error case - invalid input") {
        // Arrange
        // ... setup for error case
        
        // Act & Assert
        REQUIRE_THROWS_AS(
            obj->method(invalid_param),
            std::invalid_argument);
    }
}

}  // namespace smpp
```

---

## Mocking Strategy

### Mock Objects for Dependencies

Use simple mock implementations (not a mocking framework like Google Mock).

```cpp
// mocks/mock_dbus_authenticator.hpp
#ifndef TESTS_MOCKS_MOCK_DBUS_AUTHENTICATOR_HPP_
#define TESTS_MOCKS_MOCK_DBUS_AUTHENTICATOR_HPP_

#include "authenticators/dbus_authenticator.hpp"

namespace smpp {

class MockDBusAuthenticator : public DBusAuthenticator {
public:
    MockDBusAuthenticator()
        : DBusAuthenticator("mock", true) {}
    
    // Override to not actually call D-Bus
    AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        const std::string& client_ip) const override {
        
        if (username == "valid_user" && password == "valid_pass") {
            return { true, "OK" };
        } else {
            return { false, "Invalid credentials" };
        }
    }
};

}  // namespace smpp

#endif  // TESTS_MOCKS_MOCK_DBUS_AUTHENTICATOR_HPP_
```

### Capture Behavior

```cpp
// mocks/mock_logger.hpp
#ifndef TESTS_MOCKS_MOCK_LOGGER_HPP_
#define TESTS_MOCKS_MOCK_LOGGER_HPP_

#include "logger.hpp"
#include <vector>
#include <string>

namespace smpp {

class MockLogger : public Logger {
public:
    std::vector<std::pair<std::string, std::string>> logs;  // (level, message)
    
    void log_info(const std::string& msg) override {
        logs.push_back({"info", msg});
    }
    
    void log_warn(const std::string& msg) override {
        logs.push_back({"warn", msg});
    }
    
    void log_error(const std::string& msg) override {
        logs.push_back({"error", msg});
    }
    
    // Check if specific message was logged
    bool was_logged(const std::string& msg) const {
        for (const auto& [level, message] : logs) {
            if (message.find(msg) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

}  // namespace smpp

#endif  // TESTS_MOCKS_MOCK_LOGGER_HPP_
```

---

## Test Coverage Guidelines

Aim for ~65% coverage in unit tests:

| Class | Coverage Target | Key Scenarios |
|---|---|---|
| SmppMessage | 100% | encode/decode roundtrip, field extraction, edge cases |
| SmppSession | 100% | all state transitions, invalid transitions, queries |
| SmppMessageParser | 100% | chunked input, multiple PDUs, incomplete messages |
| SmppMessageEncoder | 90% | all response types, status codes, body format |
| IpValidator | 90% | whitelist loading, exact match, CIDR match, rejection |
| CredentialValidator | 90% | valid/invalid formats, C-string parsing, bounds |
| DBusAuthenticator | 70% | successful auth, failed auth, D-Bus error handling |
| Logger | 70% | initialization, both sinks, level filtering |
| SessionManager | 85% | create/get/remove, thread safety, cleanup |
| Handlers (5×) | 80% each | valid request, invalid state, auth failure, exceptions |
| SmppMessageProcessor | 80% | routing, exception handling, all command IDs |

---

## Example: SmppMessage Unit Tests

```cpp
#include <catch2/catch_test_macros.hpp>
#include "smpp_message.hpp"

namespace smpp {

TEST_CASE("SmppMessage", "[smpp_message]") {
    
    SECTION("Construction") {
        uint32_t cmd_id = 0x00000002;  // BIND_TRANSMITTER
        uint32_t status = 0x00000000;
        uint32_t seq = 42;
        std::vector<uint8_t> body = { 't', 'e', 's', 't', '\0' };
        
        SmppMessage msg(cmd_id, status, seq, body);
        
        REQUIRE(msg.command_id() == cmd_id);
        REQUIRE(msg.command_status() == status);
        REQUIRE(msg.sequence_number() == seq);
        REQUIRE(msg.body() == body);
    }
    
    SECTION("Type queries") {
        SmppMessage bind_tx(0x00000002, 0, 1);
        SmppMessage bind_resp(0x80000002, 0, 1);
        
        REQUIRE(bind_tx.is_bind_transmitter());
        REQUIRE(bind_resp.is_bind_response());
        REQUIRE(!bind_tx.is_bind_response());
    }
    
    SECTION("Encode produces valid PDU") {
        SmppMessage msg(0x00000002, 0, 1, { 'a', 'b' });
        auto encoded = msg.encode();
        
        // Check length (16 header + 2 body)
        REQUIRE(encoded.size() == 18);
        
        // Check command_length (big-endian)
        REQUIRE(encoded[0] == 0);
        REQUIRE(encoded[1] == 0);
        REQUIRE(encoded[2] == 0);
        REQUIRE(encoded[3] == 18);
    }
    
    SECTION("Decode roundtrip") {
        std::vector<uint8_t> original = {
            0, 0, 0, 18,              // command_length = 18
            0, 0, 0, 2,               // command_id = BIND_TRANSMITTER
            0, 0, 0, 0,               // command_status = OK
            0, 0, 0, 1,               // sequence_number = 1
            'a', 'b'                  // body
        };
        
        auto decoded = SmppMessage::decode(original);
        
        REQUIRE(decoded.command_id() == 0x00000002);
        REQUIRE(decoded.command_status() == 0);
        REQUIRE(decoded.sequence_number() == 1);
        REQUIRE(decoded.body() == std::vector<uint8_t>{ 'a', 'b' });
        
        // Re-encode and verify matches
        REQUIRE(decoded.encode() == original);
    }
    
    SECTION("Field extraction - get_system_id") {
        std::vector<uint8_t> body = { 't', 'e', 's', 't', '\0', 'x', 'y', 'z' };
        SmppMessage msg(0, 0, 0, body);
        
        REQUIRE(msg.get_system_id() == "test");
    }
    
    SECTION("Empty body") {
        SmppMessage msg(0, 0, 0);  // No body
        auto encoded = msg.encode();
        
        // Should be exactly 16 bytes
        REQUIRE(encoded.size() == 16);
    }
}

}  // namespace smpp
```

---

## Example: SmppSession Unit Tests

```cpp
#include <catch2/catch_test_macros.hpp>
#include "smpp_session.hpp"

namespace smpp {

TEST_CASE("SmppSession", "[smpp_session]") {
    SmppSession session("sess_1", "127.0.0.1");
    
    SECTION("Initial state is UNBOUND") {
        REQUIRE(session.state() == SmppSession::State::UNBOUND);
        REQUIRE(!session.is_bound());
        REQUIRE(!session.can_transmit());
        REQUIRE(!session.can_receive());
    }
    
    SECTION("Bind as transmitter succeeds from UNBOUND") {
        REQUIRE(session.try_bind_as_transmitter("user123"));
        REQUIRE(session.state() == SmppSession::State::BOUND_TRANSMITTER);
        REQUIRE(session.is_bound());
        REQUIRE(session.can_transmit());
        REQUIRE(!session.can_receive());
        REQUIRE(session.authenticated_as() == "user123");
    }
    
    SECTION("Cannot bind twice") {
        REQUIRE(session.try_bind_as_transmitter("user1"));
        REQUIRE(!session.try_bind_as_transmitter("user2"));  // ← fails
        REQUIRE(session.authenticated_as() == "user1");  // ← unchanged
    }
    
    SECTION("Cannot bind receiver after transmitter bind") {
        REQUIRE(session.try_bind_as_transmitter("user"));
        REQUIRE(!session.try_bind_as_receiver("user"));  // ← fails
    }
    
    SECTION("Unbind succeeds from any BOUND state") {
        session.try_bind_as_transmitter("user");
        REQUIRE(session.try_unbind());
        REQUIRE(session.state() == SmppSession::State::UNBOUND);
    }
    
    SECTION("Cannot unbind from UNBOUND") {
        REQUIRE(!session.try_unbind());  // ← fails, no state change
    }
    
    SECTION("Touch updates last_activity") {
        auto before = session.last_activity();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        session.touch();
        auto after = session.last_activity();
        
        REQUIRE(after > before);
    }
    
    SECTION("Thread-safe access") {
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 10; i++) {
            threads.emplace_back([&session, i]() {
                if (i == 0) {
                    session.try_bind_as_transmitter("user");
                } else {
                    session.is_bound();  // Concurrent read
                    session.touch();
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        REQUIRE(session.is_bound());
    }
}

}  // namespace smpp
```

---

## Running Unit Tests

```bash
# Build
cd /workspace
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run all unit tests
ctest --build-dir build --output-on-failure

# Run specific test
build/tests/unit/test_smpp_message --success

# Run with verbose output
ctest --build-dir build -V

# Filter tests by name
ctest --build-dir build -R "SmppSession" --output-on-failure
```

---

## Coverage Goals

Aim for these coverage percentages:

| Metric | Target |
|---|---|
| Unit test coverage | 65% |
| Integration test coverage | 25% |
| E2E test coverage | 10% |
| **Total** | **100%** |

Use `gcov` or `lcov` to measure:

```bash
cmake -B build -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
cmake --build build
ctest --build-dir build
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
# Open coverage_html/index.html in browser
```

---

**Next**: integration_testing_strategy.md
