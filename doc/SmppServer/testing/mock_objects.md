# Mock Objects for Testing

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: Test doubles and mock implementations  

---

## Mock Objects Location

```
tests/mocks/
├── mock_dbus_authenticator.hpp
├── mock_logger.hpp
├── mock_ip_validator.hpp
├── mock_credential_validator.hpp
├── mock_session_manager.hpp
├── mock_smpp_message_processor.hpp
└── mock_socket.hpp
```

---

## Mock Implementations

### MockDBusAuthenticator

```cpp
#ifndef TESTS_MOCKS_MOCK_DBUS_AUTHENTICATOR_HPP_
#define TESTS_MOCKS_MOCK_DBUS_AUTHENTICATOR_HPP_

#include "authenticators/dbus_authenticator.hpp"

namespace smpp {

class MockDBusAuthenticator {
public:
    struct AuthResult {
        bool success;
        std::string message;
    };
    
    MockDBusAuthenticator()
        : default_success_(true) {}
    
    // Set test behavior
    void set_success(bool success) { default_success_ = success; }
    void set_authorized_user(const std::string& user) { authorized_user_ = user; }
    void set_authorized_password(const std::string& pass) { authorized_pass_ = pass; }
    
    // Capture behavior
    struct CallRecord {
        std::string username;
        std::string password;
        std::string client_ip;
    };
    
    std::vector<CallRecord> call_history;
    
    // Implementation
    AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        const std::string& client_ip) const {
        
        call_history.push_back({ username, password, client_ip });
        
        if (!authorized_user_.empty() && username != authorized_user_) {
            return { false, "Invalid user" };
        }
        
        if (!authorized_pass_.empty() && password != authorized_pass_) {
            return { false, "Invalid password" };
        }
        
        return { default_success_, "OK" };
    }
    
    size_t call_count() const { return call_history.size(); }
    bool was_called_with(const std::string& user) const {
        for (const auto& call : call_history) {
            if (call.username == user) return true;
        }
        return false;
    }

private:
    bool default_success_;
    std::string authorized_user_;
    std::string authorized_pass_;
    mutable std::vector<CallRecord> call_history;
};

}  // namespace smpp

#endif
```

### MockLogger

```cpp
#ifndef TESTS_MOCKS_MOCK_LOGGER_HPP_
#define TESTS_MOCKS_MOCK_LOGGER_HPP_

#include <vector>
#include <string>

namespace smpp {

class MockLogger {
public:
    struct LogEntry {
        std::string level;
        std::string message;
    };
    
    // Capture logs
    std::vector<LogEntry> logs;
    
    void log_info(const std::string& msg) {
        logs.push_back({"info", msg});
    }
    
    void log_warn(const std::string& msg) {
        logs.push_back({"warn", msg});
    }
    
    void log_error(const std::string& msg) {
        logs.push_back({"error", msg});
    }
    
    void log_critical(const std::string& msg) {
        logs.push_back({"critical", msg});
    }
    
    // Queries
    size_t log_count() const { return logs.size(); }
    
    bool contains(const std::string& msg) const {
        for (const auto& entry : logs) {
            if (entry.message.find(msg) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    bool contains_at_level(const std::string& level, const std::string& msg) const {
        for (const auto& entry : logs) {
            if (entry.level == level && entry.message.find(msg) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    void clear() { logs.clear(); }
};

}  // namespace smpp

#endif
```

### MockIpValidator

```cpp
#ifndef TESTS_MOCKS_MOCK_IP_VALIDATOR_HPP_
#define TESTS_MOCKS_MOCK_IP_VALIDATOR_HPP_

#include <string>
#include <set>

namespace smpp {

class MockIpValidator {
public:
    void whitelist(const std::string& ip) {
        whitelist_.insert(ip);
    }
    
    void blacklist(const std::string& ip) {
        whitelist_.erase(ip);
    }
    
    bool is_whitelisted(const std::string& client_ip) const {
        return whitelist_.count(client_ip) > 0;
    }
    
    size_t whitelist_size() const {
        return whitelist_.size();
    }

private:
    std::set<std::string> whitelist_;
};

}  // namespace smpp

#endif
```

### MockCredentialValidator

```cpp
#ifndef TESTS_MOCKS_MOCK_CREDENTIAL_VALIDATOR_HPP_
#define TESTS_MOCKS_MOCK_CREDENTIAL_VALIDATOR_HPP_

#include <string>

namespace smpp {

class MockCredentialValidator {
public:
    struct ValidationResult {
        bool valid;
        std::string username;
        std::string password;
        std::string error_message;
    };
    
    void allow_user(const std::string& username) {
        allowed_user_ = username;
    }
    
    ValidationResult validate_credentials(
        const std::string& username,
        const std::string& password) const {
        
        if (username != allowed_user_) {
            return { false, username, password, "Invalid user" };
        }
        
        return { true, username, password, "" };
    }
    
    ValidationResult extract_from_bind_body(
        const std::vector<uint8_t>& body) const {
        
        // Simple parse: first C-string, second C-string
        if (body.empty()) {
            return { false, "", "", "Empty body" };
        }
        
        size_t null_pos = 0;
        while (null_pos < body.size() && body[null_pos] != '\0') {
            null_pos++;
        }
        
        if (null_pos >= body.size()) {
            return { false, "", "", "No null terminator" };
        }
        
        std::string username(
            reinterpret_cast<const char*>(body.data()),
            null_pos);
        
        return validate_credentials(username, "");
    }

private:
    std::string allowed_user_ = "test";
};

}  // namespace smpp

#endif
```

### MockSessionManager

```cpp
#ifndef TESTS_MOCKS_MOCK_SESSION_MANAGER_HPP_
#define TESTS_MOCKS_MOCK_SESSION_MANAGER_HPP_

#include <memory>
#include <map>
#include "smpp_session.hpp"

namespace smpp {

class MockSessionManager {
public:
    std::shared_ptr<SmppSession> create_session(const std::string& client_ip) {
        auto session = std::make_shared<SmppSession>(
            "sess_" + std::to_string(++next_id_),
            client_ip);
        
        sessions_[session->session_id()] = session;
        return session;
    }
    
    std::shared_ptr<SmppSession> get_session(const std::string& session_id) const {
        auto it = sessions_.find(session_id);
        return (it != sessions_.end()) ? it->second : nullptr;
    }
    
    void remove_session(const std::string& session_id) {
        sessions_.erase(session_id);
    }
    
    size_t active_count() const {
        return sessions_.size();
    }

private:
    std::map<std::string, std::shared_ptr<SmppSession>> sessions_;
    static uint64_t next_id_;
};

uint64_t MockSessionManager::next_id_ = 0;

}  // namespace smpp

#endif
```

### MockSmppMessageProcessor

```cpp
#ifndef TESTS_MOCKS_MOCK_SMPP_MESSAGE_PROCESSOR_HPP_
#define TESTS_MOCKS_MOCK_SMPP_MESSAGE_PROCESSOR_HPP_

#include "smpp_message.hpp"
#include "smpp_session.hpp"

namespace smpp {

class MockSmppMessageProcessor {
public:
    // Configure response for testing
    void set_response(const SmppMessage& response) {
        response_ = response;
    }
    
    SmppMessage process_message(
        const SmppMessage& request,
        SmppSession& session) {
        
        // Capture the request
        last_request_ = request;
        last_request_seq_ = request.sequence_number();
        
        // Return configured response
        return response_;
    }
    
    // Queries
    uint32_t last_request_seq() const {
        return last_request_seq_;
    }
    
    const SmppMessage& last_request() const {
        return last_request_;
    }

private:
    SmppMessage response_;
    SmppMessage last_request_;
    uint32_t last_request_seq_ = 0;
};

}  // namespace smpp

#endif
```

### MockSocket (ASIO)

For testing ASIO-dependent code without real sockets:

```cpp
#ifndef TESTS_MOCKS_MOCK_SOCKET_HPP_
#define TESTS_MOCKS_MOCK_SOCKET_HPP_

#include <asio.hpp>
#include <vector>

namespace smpp {

// Stub socket for testing (minimal implementation)
// Full mock ASIO socket is complex; for real integration tests,
// use localhost connection or mock at SmppConnection level

class MockSocketStub {
public:
    void set_data(const std::vector<uint8_t>& data) {
        data_ = data;
        read_pos_ = 0;
    }
    
    std::vector<uint8_t> read(size_t max_bytes) {
        std::vector<uint8_t> result;
        while (result.size() < max_bytes && read_pos_ < data_.size()) {
            result.push_back(data_[read_pos_++]);
        }
        return result;
    }
    
    void write(const std::vector<uint8_t>& data) {
        written_data_.insert(written_data_.end(), data.begin(), data.end());
    }
    
    const std::vector<uint8_t>& get_written() const {
        return written_data_;
    }
    
    void clear() {
        data_.clear();
        written_data_.clear();
        read_pos_ = 0;
    }

private:
    std::vector<uint8_t> data_;
    std::vector<uint8_t> written_data_;
    size_t read_pos_ = 0;
};

}  // namespace smpp

#endif
```

---

## Using Mocks in Tests

### Example: Handler Unit Test with Mock Authenticator

```cpp
#include <catch2/catch_test_macros.hpp>
#include "handlers/bind_transmitter_handler.hpp"
#include "mocks/mock_dbus_authenticator.hpp"
#include "mocks/mock_logger.hpp"

namespace smpp {

TEST_CASE("BindTransmitterHandler", "[handler][bind_transmitter]") {
    auto mock_auth = std::make_shared<MockDBusAuthenticator>();
    auto mock_logger = std::make_shared<MockLogger>();
    
    auto handler = std::make_shared<BindTransmitterHandler>(
        mock_auth,
        mock_logger);
    
    SECTION("Accepts valid credentials") {
        mock_auth->set_authorized_user("test");
        mock_auth->set_authorized_password("pass");
        
        SmppMessage request(BIND_TRANSMITTER, 0, 1,
            std::vector<uint8_t>{ 't','e','s','t','\0','p','a','s','s','\0' });
        
        SmppSession session("s1", "127.0.0.1");
        
        auto response = handler->handle(request, session);
        
        REQUIRE(response.command_status() == ESME_ROK);
        REQUIRE(session.is_bound());
        REQUIRE(mock_auth->call_count() == 1);
        REQUIRE(mock_auth->was_called_with("test"));
    }
    
    SECTION("Rejects invalid credentials") {
        mock_auth->set_success(false);
        
        SmppMessage request(BIND_TRANSMITTER, 0, 1,
            std::vector<uint8_t>{ 'f','a','k','e','\0','w','r','o','n','g','\0' });
        
        SmppSession session("s2", "127.0.0.1");
        
        auto response = handler->handle(request, session);
        
        REQUIRE(response.command_status() == ESME_RINVPASWD);
        REQUIRE(!session.is_bound());
    }
}

}  // namespace smpp
```

---

## Stub vs Mock

| Type | Purpose | When to Use |
|---|---|---|
| **Stub** | Return fixed values, minimal behavior | Testing a single class in isolation |
| **Mock** | Capture method calls, verify interactions | Testing inter-class communication |
| **Fake** | Working implementation, slower | Close to real behavior but testable |

**Example**:
- **Stub Logger**: Always returns immediately, no actual logging
- **Mock Logger**: Captures all log calls so test can verify correct messages were logged
- **Fake Logger**: Actually writes to file (slow, but close to real behavior)

For SMPPServer, use **Mocks** (capture behavior) and **Stubs** (minimal responses).

---

**Next**: build_system and error_handling documents
