# Header File Templates

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: Boilerplate for .hpp files  

---

## Standard Header File Template

Use this template for all new .hpp files:

```cpp
#ifndef SMPPSERVER_[PATH_]CLASS_NAME_HPP_
#define SMPPSERVER_[PATH_]CLASS_NAME_HPP_

#include <vector>
#include <string>
#include <memory>
#include <mutex>

#include "smpp_message.hpp"
#include "logger.hpp"

namespace smpp {

/**
 * Brief description of class responsibility (1-2 sentences).
 * 
 * Longer description explaining:
 * - What problem this class solves
 * - How it interacts with other classes
 * - Key design decisions
 */
class ClassName {
public:
    /**
     * Constructor.
     * 
     * @param dependency1 First injected dependency, brief explanation
     * @param dependency2 Second injected dependency
     */
    ClassName(
        std::shared_ptr<Dependency1> dependency1,
        std::shared_ptr<Dependency2> dependency2);
    
    /**
     * Public method that does X.
     * 
     * @param param1 Parameter explanation
     * @return Return value explanation
     * @throws std::exception If something goes wrong
     */
    ReturnType public_method(const std::string& param1);
    
    /**
     * Query method for state.
     * 
     * @return Current state or value
     */
    int get_state() const;
    
private:
    // Member variables
    std::shared_ptr<Dependency1> dependency1_;
    std::shared_ptr<Dependency2> dependency2_;
    std::string internal_state_;
    mutable std::mutex state_mutex_;
    
    // Private helper methods (declare but don't document publicly)
    void helper_method_();
};

}  // namespace smpp

#endif  // SMPPSERVER_[PATH_]CLASS_NAME_HPP_
```

---

## Naming Conventions

### Header Guards
- Format: `SMPPSERVER_[RELATIVE_PATH_]CLASS_NAME_HPP_`
- Example: `SMPPSERVER_HANDLERS_BIND_TRANSMITTER_HANDLER_HPP_`
- All uppercase, underscores, trailing underscore before `HPP_`

### Include Order
1. System headers (`<vector>`, `<string>`, `<memory>`, etc.)
2. External headers (`<asio.hpp>`, `<spdlog/spdlog.h>`)
3. Project headers (`"smpp_message.hpp"`, `"logger.hpp"`)

Blank line between each section.

### Namespaces
All classes in namespace `smpp`:

```cpp
namespace smpp {

class SmppMessage { ... };
class SmppSession { ... };

}  // namespace smpp
```

### Member Variable Names
All private members end with underscore: `member_name_`

Example:
```cpp
private:
    std::shared_ptr<Logger> logger_;
    std::vector<uint8_t> buffer_;
    bool is_connected_;
```

---

## Documentation Style

### Class-Level Doxygen Comment

```cpp
/**
 * Brief one-liner (what the class does).
 * 
 * Detailed description:
 * - Main responsibility
 * - How it's used
 * - Key design decisions
 * 
 * Thread-safety note (if relevant):
 * Thread-safe: all methods protected by mutex_
 * OR
 * NOT thread-safe: caller must serialize access
 * 
 * Example usage:
 * @code
 *   auto session = std::make_shared<SmppSession>("sess_1", "127.0.0.1");
 *   session->try_bind_as_transmitter("user");
 *   if (session->is_bound()) { ... }
 * @endcode
 */
```

### Public Method Documentation

```cpp
/**
 * Brief description of what this method does.
 * 
 * Longer explanation if behavior is non-obvious:
 * - When to call it
 * - What state changes occur
 * - Side effects (logging, D-Bus calls, etc.)
 * 
 * @param param1 What this parameter means, constraints
 * @param param2 Another parameter
 * @return What is returned, meaning of return value
 * @throws std::invalid_argument If param1 is invalid
 * @throws std::runtime_error If D-Bus call fails
 */
ReturnType public_method(
    const std::string& param1,
    int param2);
```

### No Private Method Documentation
Private helper methods do NOT need Doxygen comments. Just clear names:

```cpp
private:
    // OK: clear name, no comment needed
    std::vector<uint8_t> parse_header_();
    
    // NOT OK: needs a comment because name is unclear
    void x_();  // ← bad naming, would need comment
```

---

## Constructor Pattern

Standard dependency injection constructor:

```cpp
class SmppConnection {
public:
    SmppConnection(
        asio::ip::tcp::socket socket,
        std::shared_ptr<SmppSession> session,
        std::shared_ptr<SmppMessageProcessor> message_processor,
        std::shared_ptr<Logger> logger);
    
    // NO default constructor
    SmppConnection() = delete;
    
    // NO copy constructor
    SmppConnection(const SmppConnection&) = delete;
    SmppConnection& operator=(const SmppConnection&) = delete;
    
    // NO move constructor (for ASIO objects that hold non-movable state)
    // OR allow move if safe
    SmppConnection(SmppConnection&&) = default;
    SmppConnection& operator=(SmppConnection&&) = default;

private:
    // Member variables in declaration order
    asio::ip::tcp::socket socket_;
    std::shared_ptr<SmppSession> session_;
    std::shared_ptr<SmppMessageProcessor> message_processor_;
    std::shared_ptr<Logger> logger_;
};
```

---

## Access Level Ordering

Always: public → private

Within each section:
1. Type declarations (using, enum, struct)
2. Constructor/destructor
3. Public methods (queries first, then actions)
4. Private data members
5. Private helper methods

```cpp
class Example {
public:
    // Types
    enum class State { UNBOUND, BOUND };
    
    // Constructor
    Example(std::shared_ptr<Logger> logger);
    
    // Query methods (const)
    State state() const;
    bool is_bound() const;
    
    // Action methods (non-const)
    bool try_bind(const std::string& user);
    void unbind();
    
private:
    // Data members
    State state_;
    std::string username_;
    std::shared_ptr<Logger> logger_;
    mutable std::mutex state_mutex_;
    
    // Helper methods
    void log_state_change_(State old_state, State new_state);
};
```

---

## Template Examples

### Simple Value Object (SmppMessage)

```cpp
#ifndef SMPPSERVER_SMPP_MESSAGE_HPP_
#define SMPPSERVER_SMPP_MESSAGE_HPP_

#include <vector>
#include <string>
#include <cstdint>

namespace smpp {

/**
 * Immutable value object representing one SMPP PDU.
 * 
 * Stores: command_id, command_status, sequence_number, and variable body.
 * All fields are const-accessible. No modifications after construction.
 * 
 * Thread-safe: immutable object, no synchronization needed.
 */
class SmppMessage {
public:
    /**
     * Constructor.
     * 
     * @param command_id SMPP command identifier (4 bytes, big-endian)
     * @param command_status Response status code (0 = success, non-zero = error)
     * @param sequence_number Request/response sequence number
     * @param body Variable-length body (default empty)
     */
    SmppMessage(
        uint32_t command_id,
        uint32_t command_status,
        uint32_t sequence_number,
        const std::vector<uint8_t>& body = {});
    
    // Accessors (all const, immutable)
    uint32_t command_id() const;
    uint32_t command_status() const;
    uint32_t sequence_number() const;
    const std::vector<uint8_t>& body() const;
    
    // Type queries
    bool is_bind_transmitter() const;
    bool is_bind_receiver() const;
    bool is_bind_transceiver() const;
    bool is_bind_response() const;
    bool is_unbind() const;
    bool is_enquire_link() const;
    
    // Field extraction (for BIND messages)
    std::string get_system_id() const;
    std::string get_password() const;
    
    // Serialization
    std::vector<uint8_t> encode() const;
    
    // Factory method
    static SmppMessage decode(const std::vector<uint8_t>& bytes);
    
private:
    uint32_t command_id_;
    uint32_t command_status_;
    uint32_t sequence_number_;
    std::vector<uint8_t> body_;
};

}  // namespace smpp

#endif  // SMPPSERVER_SMPP_MESSAGE_HPP_
```

### State Machine (SmppSession)

```cpp
#ifndef SMPPSERVER_SMPP_SESSION_HPP_
#define SMPPSERVER_SMPP_SESSION_HPP_

#include <string>
#include <ctime>

namespace smpp {

/**
 * Connection state tracker for one SMPP client.
 * 
 * Enforces valid state transitions:
 * UNBOUND → BOUND_TRANSMITTER/RECEIVER/TRANSCEIVER → UNBOUND → CLOSED
 * 
 * All try_* methods return false if transition is invalid.
 * 
 * Thread-safe: all state access is synchronized with mutex.
 */
class SmppSession {
public:
    enum class State {
        UNBOUND,
        BOUND_TRANSMITTER,
        BOUND_RECEIVER,
        BOUND_TRANSCEIVER,
        CLOSED
    };
    
    /**
     * Constructor.
     * 
     * @param session_id Unique identifier for this session
     * @param client_ip IP address of connecting client
     */
    SmppSession(
        const std::string& session_id,
        const std::string& client_ip);
    
    // Accessors (read-only)
    const std::string& session_id() const;
    const std::string& client_ip() const;
    State state() const;
    const std::string& authenticated_as() const;
    std::time_t connected_at() const;
    std::time_t last_activity() const;
    
    // State queries
    bool is_bound() const;
    bool can_transmit() const;
    bool can_receive() const;
    
    // State transitions (return false if invalid)
    bool try_bind_as_transmitter(const std::string& username);
    bool try_bind_as_receiver(const std::string& username);
    bool try_bind_as_transceiver(const std::string& username);
    bool try_unbind();
    bool try_close();
    
    // Activity tracking
    void touch();
    
private:
    std::string session_id_;
    std::string client_ip_;
    State state_;
    std::string authenticated_as_;
    std::time_t connected_at_;
    std::time_t last_activity_;
    mutable std::mutex state_mutex_;
};

}  // namespace smpp

#endif  // SMPPSERVER_SMPP_SESSION_HPP_
```

### Handler (Strategy Pattern)

```cpp
#ifndef SMPPSERVER_HANDLERS_BIND_TRANSMITTER_HANDLER_HPP_
#define SMPPSERVER_HANDLERS_BIND_TRANSMITTER_HANDLER_HPP_

#include <memory>
#include "smpp_request_handler.hpp"
#include "authenticators/dbus_authenticator.hpp"
#include "logger.hpp"

namespace smpp {

/**
 * Handler for BIND_TRANSMITTER requests.
 * 
 * Validates credentials via DBusAuthenticator and sets session state.
 * Stateless: can be reused for multiple clients.
 */
class BindTransmitterHandler : public SmppRequestHandler {
public:
    /**
     * Constructor.
     * 
     * @param dbus_auth D-Bus bridge for credential validation
     * @param logger Logging service
     */
    BindTransmitterHandler(
        std::shared_ptr<DBusAuthenticator> dbus_auth,
        std::shared_ptr<Logger> logger);
    
    SmppMessage handle(
        const SmppMessage& request,
        SmppSession& session) override;
    
private:
    std::shared_ptr<DBusAuthenticator> dbus_auth_;
    std::shared_ptr<Logger> logger_;
};

}  // namespace smpp

#endif  // SMPPSERVER_HANDLERS_BIND_TRANSMITTER_HANDLER_HPP_
```

---

## Checklist Before Commit

- [ ] Header guard is correct format and unique
- [ ] All includes are necessary and in correct order
- [ ] Class in `smpp` namespace
- [ ] Constructor with dependency injection
- [ ] All public methods documented with Doxygen
- [ ] Private members end with `_`
- [ ] No public data members (use accessors)
- [ ] Const-correctness applied (queries are `const`)
- [ ] Constructor is non-trivial; no default constructor (unless needed)
- [ ] Copy/move semantics explicitly deleted or defined
- [ ] No circular includes
- [ ] Compiles with `-Wall -Wextra` (no warnings)

---

**Next**: cpp_file_templates.md
