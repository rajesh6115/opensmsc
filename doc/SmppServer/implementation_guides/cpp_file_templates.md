# C++ Implementation File Templates

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: Boilerplate for .cpp files  

---

## Standard C++ File Template

Use this template for all new .cpp files:

```cpp
#include "class_name.hpp"

#include <stdexcept>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace smpp {

// Constructor
ClassName::ClassName(
    std::shared_ptr<Dependency1> dependency1,
    std::shared_ptr<Dependency2> dependency2)
    : dependency1_(std::move(dependency1)),
      dependency2_(std::move(dependency2)),
      internal_state_(0) {
}

// Public methods
ReturnType ClassName::public_method(const std::string& param1) {
    if (param1.empty()) {
        throw std::invalid_argument("param1 cannot be empty");
    }
    
    // Method implementation
    return result;
}

int ClassName::get_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return internal_state_;
}

// Private helper methods
void ClassName::helper_method_() {
    // Implementation
}

}  // namespace smpp
```

---

## Include Order in .cpp Files

1. Own header: `#include "class_name.hpp"`
2. Blank line
3. Standard library headers: `<vector>`, `<string>`, `<stdexcept>`, etc.
4. External library headers: `<asio.hpp>`, `<spdlog/spdlog.h>`, etc.
5. Project headers: `"other_class.hpp"`, `"logger.hpp"`, etc.

Blank line between each section.

```cpp
#include "smpp_message.hpp"

#include <vector>
#include <string>
#include <algorithm>
#include <spdlog/spdlog.h>

#include "smpp_session.hpp"
#include "logger.hpp"

namespace smpp {
    // ...
}
```

---

## Constructor Implementation

Always move dependencies in initializer list:

```cpp
ClassName::ClassName(
    std::shared_ptr<Dependency1> dep1,
    std::shared_ptr<Dependency2> dep2)
    : dep1_(std::move(dep1)),
      dep2_(std::move(dep2)),
      state_(InitialState),
      internal_counter_(0) {
    
    // Only do non-trivial initialization in body
    if (!dep1_) {
        throw std::invalid_argument("dep1 is null");
    }
}
```

**Key rules**:
- Initialize member variables in same order as declared in .hpp
- Use `std::move()` for shared_ptr parameters
- Throw exceptions for null dependencies
- Keep constructor body minimal (complex logic goes in separate methods)

---

## Error Handling Pattern

### Validation at Entry

```cpp
bool SmppSession::try_bind_as_transmitter(const std::string& username) {
    // Validate input
    if (username.empty()) {
        spdlog::warn("try_bind_as_transmitter: empty username");
        return false;
    }
    
    // Validate state
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ != State::UNBOUND) {
        spdlog::warn("try_bind: invalid state {}", static_cast<int>(state_));
        return false;
    }
    
    // Perform state change
    state_ = State::BOUND_TRANSMITTER;
    authenticated_as_ = username;
    touch();
    
    spdlog::info("Client bound as transmitter: {}", username);
    return true;
}
```

### Exception Handling

```cpp
SmppMessage SmppMessageProcessor::process_message(
    const SmppMessage& request,
    SmppSession& session) {
    
    try {
        // Normal processing
        switch (request.command_id()) {
        case BIND_TRANSMITTER:
            return handle_bind_transmitter(request, session);
        case UNBIND:
            return handle_unbind(request, session);
        default:
            spdlog::warn("Unknown command: 0x{:08x}", request.command_id());
            return SmppMessageEncoder::build_generic_resp(
                request.command_id(),
                ESME_RINVCMDID,  // Invalid command
                request.sequence_number());
        }
    } catch (const std::exception& ex) {
        spdlog::error("Exception in message processor: {}", ex.what());
        return SmppMessageEncoder::build_generic_resp(
            request.command_id(),
            ESME_RSUBMITFAIL,  // Generic error
            request.sequence_number());
    }
}
```

---

## Thread Safety Pattern

Use `std::lock_guard` or `std::shared_lock` consistently:

```cpp
State SmppSession::state() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    return state_;
}

bool SmppSession::try_bind_as_transmitter(const std::string& username) {
    std::lock_guard<std::shared_mutex> lock(state_mutex_);
    
    if (state_ != State::UNBOUND) {
        return false;
    }
    
    state_ = State::BOUND_TRANSMITTER;
    authenticated_as_ = username;
    return true;
}
```

**Rules**:
- Use `std::shared_lock` for query methods that only read (`const` methods)
- Use `std::lock_guard` for modification methods (non-const)
- Acquire lock at the beginning of the critical section
- Lock is automatically released when `lock_guard`/`shared_lock` goes out of scope
- Never hold mutex across I/O operations (e.g., D-Bus calls, file writes)

---

## Template Examples

### Value Object Implementation (SmppMessage)

```cpp
#include "smpp_message.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace smpp {

SmppMessage::SmppMessage(
    uint32_t command_id,
    uint32_t command_status,
    uint32_t sequence_number,
    const std::vector<uint8_t>& body)
    : command_id_(command_id),
      command_status_(command_status),
      sequence_number_(sequence_number),
      body_(body) {
}

uint32_t SmppMessage::command_id() const {
    return command_id_;
}

uint32_t SmppMessage::command_status() const {
    return command_status_;
}

uint32_t SmppMessage::sequence_number() const {
    return sequence_number_;
}

const std::vector<uint8_t>& SmppMessage::body() const {
    return body_;
}

bool SmppMessage::is_bind_transmitter() const {
    return command_id_ == 0x00000002;
}

bool SmppMessage::is_bind_receiver() const {
    return command_id_ == 0x00000001;
}

bool SmppMessage::is_bind_transceiver() const {
    return command_id_ == 0x00000009;
}

bool SmppMessage::is_bind_response() const {
    return (command_id_ & 0x80000000) != 0;
}

bool SmppMessage::is_unbind() const {
    return command_id_ == 0x00000006;
}

bool SmppMessage::is_enquire_link() const {
    return command_id_ == 0x0000000F;
}

std::string SmppMessage::get_system_id() const {
    if (body_.empty()) {
        return "";
    }
    
    // Find null terminator
    auto null_pos = std::find(body_.begin(), body_.end(), '\0');
    
    if (null_pos == body_.end()) {
        return "";  // No null terminator
    }
    
    return std::string(
        reinterpret_cast<const char*>(body_.data()),
        std::distance(body_.begin(), null_pos));
}

std::vector<uint8_t> SmppMessage::encode() const {
    std::vector<uint8_t> result;
    
    // Calculate command_length (header 16 bytes + body)
    uint32_t command_length = 16 + body_.size();
    
    // Append command_length (big-endian)
    result.push_back((command_length >> 24) & 0xFF);
    result.push_back((command_length >> 16) & 0xFF);
    result.push_back((command_length >> 8) & 0xFF);
    result.push_back(command_length & 0xFF);
    
    // Append command_id (big-endian)
    result.push_back((command_id_ >> 24) & 0xFF);
    result.push_back((command_id_ >> 16) & 0xFF);
    result.push_back((command_id_ >> 8) & 0xFF);
    result.push_back(command_id_ & 0xFF);
    
    // Append command_status (big-endian)
    result.push_back((command_status_ >> 24) & 0xFF);
    result.push_back((command_status_ >> 16) & 0xFF);
    result.push_back((command_status_ >> 8) & 0xFF);
    result.push_back(command_status_ & 0xFF);
    
    // Append sequence_number (big-endian)
    result.push_back((sequence_number_ >> 24) & 0xFF);
    result.push_back((sequence_number_ >> 16) & 0xFF);
    result.push_back((sequence_number_ >> 8) & 0xFF);
    result.push_back(sequence_number_ & 0xFF);
    
    // Append body
    result.insert(result.end(), body_.begin(), body_.end());
    
    return result;
}

SmppMessage SmppMessage::decode(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 16) {
        throw std::invalid_argument("PDU too short (need 16 bytes minimum)");
    }
    
    // Extract header fields (big-endian)
    uint32_t command_length =
        (static_cast<uint32_t>(bytes[0]) << 24) |
        (static_cast<uint32_t>(bytes[1]) << 16) |
        (static_cast<uint32_t>(bytes[2]) << 8) |
        (static_cast<uint32_t>(bytes[3]));
    
    uint32_t command_id =
        (static_cast<uint32_t>(bytes[4]) << 24) |
        (static_cast<uint32_t>(bytes[5]) << 16) |
        (static_cast<uint32_t>(bytes[6]) << 8) |
        (static_cast<uint32_t>(bytes[7]));
    
    uint32_t command_status =
        (static_cast<uint32_t>(bytes[8]) << 24) |
        (static_cast<uint32_t>(bytes[9]) << 16) |
        (static_cast<uint32_t>(bytes[10]) << 8) |
        (static_cast<uint32_t>(bytes[11]));
    
    uint32_t sequence_number =
        (static_cast<uint32_t>(bytes[12]) << 24) |
        (static_cast<uint32_t>(bytes[13]) << 16) |
        (static_cast<uint32_t>(bytes[14]) << 8) |
        (static_cast<uint32_t>(bytes[15]));
    
    // Extract body
    std::vector<uint8_t> body(bytes.begin() + 16, bytes.end());
    
    return SmppMessage(command_id, command_status, sequence_number, body);
}

}  // namespace smpp
```

### Handler Implementation

```cpp
#include "handlers/bind_transmitter_handler.hpp"

#include <spdlog/spdlog.h>
#include "smpp_message_encoder.hpp"

namespace smpp {

BindTransmitterHandler::BindTransmitterHandler(
    std::shared_ptr<DBusAuthenticator> dbus_auth,
    std::shared_ptr<Logger> logger)
    : dbus_auth_(std::move(dbus_auth)),
      logger_(std::move(logger)) {
    
    if (!dbus_auth_) {
        throw std::invalid_argument("dbus_auth is null");
    }
    if (!logger_) {
        throw std::invalid_argument("logger is null");
    }
}

SmppMessage BindTransmitterHandler::handle(
    const SmppMessage& request,
    SmppSession& session) {
    
    // Validate session state
    if (session.is_bound()) {
        spdlog::warn("BIND_TRANSMITTER: Already bound as {}", 
                     session.authenticated_as());
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_RALYBND,  // Already bound
            request.sequence_number());
    }
    
    // Extract credentials
    try {
        auto username = request.get_system_id();
        // Extract password (simplified)
        auto password = "extracted_from_body";
        
        // Validate via D-Bus
        auto auth_result = dbus_auth_->authenticate(
            username,
            password,
            session.client_ip());
        
        if (!auth_result.success) {
            spdlog::warn("Auth failed for user: {}", username);
            return SmppMessageEncoder::build_bind_transmitter_resp(
                ESME_RINVPASWD,  // Invalid password
                request.sequence_number());
        }
        
        // Update session state
        if (!session.try_bind_as_transmitter(username)) {
            spdlog::error("Failed to set session state to BOUND_TRANSMITTER");
            return SmppMessageEncoder::build_bind_transmitter_resp(
                ESME_RBINDFAIL,  // Bind failed
                request.sequence_number());
        }
        
        spdlog::info("BIND_TRANSMITTER successful: {}", username);
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_ROK,  // Success
            request.sequence_number());
        
    } catch (const std::exception& ex) {
        spdlog::error("Exception in BIND_TRANSMITTER handler: {}", ex.what());
        return SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_RSUBMITFAIL,  // Generic error
            request.sequence_number());
    }
}

}  // namespace smpp
```

---

## Checklist Before Commit

- [ ] Own header included first
- [ ] All includes are necessary
- [ ] Includes in correct order (own, std, external, project)
- [ ] All methods from .hpp are implemented
- [ ] Constructor uses initializer list and `std::move()`
- [ ] All method bodies have proper indentation
- [ ] Input validation at method entry (empty checks, null checks)
- [ ] Thread safety (locks where needed)
- [ ] Exception handling (try-catch where appropriate)
- [ ] Logging at appropriate levels (info for events, warn for invalid state, error for exceptions)
- [ ] Return values are meaningful (bool for success/failure, exception for errors)
- [ ] Compiles with `-Wall -Wextra` (no warnings)
- [ ] No memory leaks (use smart pointers, not raw `new`/`delete`)

---

**Next**: common_patterns.md
