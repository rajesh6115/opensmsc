# D-Bus Interfaces - Simple SMPP Server

Complete D-Bus interface specifications for inter-process communication in the Simple SMPP Server microservices architecture.

---

## Overview

The Simple SMPP Server uses D-Bus for inter-process communication (IPC) between independent services:

- **SMPPServer** → Provides socket claiming and server control interface
- **SMPPAuthenticator** → Provides SMPP credential authentication interface
- **SmppClientHandler** → Calls the above services via D-Bus

---

## SMPPServer Interface

**Service Name**: `com.opensmsc.SMPPServer`  
**Object Path**: `/com/opensmsc/SMPPServer`  
**Interface**: `com.opensmsc.SMPPServer`

### Methods

#### ClaimSocket(uint16 client_port) → (h socket_fd, boolean success)

Called by **SmppClientHandler** to claim ownership of a connected socket.

**Parameters**:
- `client_port` (uint16): Ephemeral port number identifying the client connection

**Returns**:
- `socket_fd` (file descriptor): The connected socket FD (transferred to caller)
- `success` (boolean): True if socket was successfully claimed

**Errors**:
- `com.opensmsc.SMPPServer.SocketNotFound`: No socket for the given port
- `com.opensmsc.SMPPServer.TransferFailed`: Socket transfer failed
- `com.opensmsc.SMPPServer.InvalidPort`: Invalid port number

**Usage Example**:
```cpp
// SmppClientHandler calls this to get its socket
DBusMessage* msg = dbus_message_new_method_call(
    "com.opensmsc.SMPPServer",           // service
    "/com/opensmsc/SMPPServer",          // object path
    "com.opensmsc.SMPPServer",           // interface
    "ClaimSocket"                        // method
);

uint16_t port = 54321;
dbus_message_append_args(msg,
    DBUS_TYPE_UINT16, &port,
    DBUS_TYPE_INVALID);

// Send and receive
DBusMessage* reply = dbus_connection_send_with_reply_and_block(
    conn, msg, -1, &error);

int socket_fd;
dbus_bool_t success;
dbus_message_get_args(reply,
    &error,
    DBUS_TYPE_UNIX_FD, &socket_fd,
    DBUS_TYPE_BOOLEAN, &success,
    DBUS_TYPE_INVALID);
```

---

#### GetConnectionCount() → (uint32 count)

Get the total number of active client connections waiting to be claimed.

**Returns**:
- `count` (uint32): Number of pending connections

**Usage**:
```cpp
// Check how many clients are waiting
uint32_t pending = server->GetConnectionCount();
```

---

#### GetConnections() → (array of (string, uint16, uint32) connections)

Get details of all active/pending connections.

**Returns**:
- `connections`: Array of tuples:
  - `client_ip` (string): IPv4 or IPv6 address
  - `client_port` (uint16): Ephemeral port number
  - `state` (uint32): 0=pending, 1=claimed, 2=disconnecting

**Usage**:
```cpp
// List all pending connections
struct Connection {
    std::string ip;
    uint16_t port;
    uint32_t state;  // 0=pending, 1=claimed
};
std::vector<Connection> conns = server->GetConnections();
```

---

#### Shutdown() → (boolean success)

Gracefully shutdown the SMPP Server.

**Returns**:
- `success` (boolean): True if shutdown initiated

**Usage**:
```cpp
// Graceful shutdown
bool shutdown_ok = server->Shutdown();
```

---

### Signals

#### ConnectionCreated(string client_ip, uint16 client_port, uint64 timestamp)

Emitted when a new client connection is accepted and IP is validated.

**Arguments**:
- `client_ip`: Client IP address
- `client_port`: Ephemeral port number
- `timestamp`: Connection time (microseconds since epoch)

---

#### ConnectionClaimed(uint16 client_port, uint32 handler_pid)

Emitted when SmppClientHandler successfully claims a socket.

**Arguments**:
- `client_port`: Port of claimed connection
- `handler_pid`: Process ID of handler that claimed it

---

#### ConnectionDropped(uint16 client_port, string reason)

Emitted when a connection is dropped or rejected.

**Arguments**:
- `client_port`: Port of dropped connection
- `reason`: Why it was dropped ("ip_rejected", "handler_crashed", "timeout", etc.)

---

### Properties

| Property | Type | Access | Description |
|----------|------|--------|-------------|
| `Port` | uint16 | read | TCP port listening on |
| `Status` | string | read | Server status: starting, running, stopping, stopped |
| `MaxConnections` | uint32 | read | Maximum concurrent connections |
| `Uptime` | uint64 | read | Server uptime in seconds |

---

## SMPPAuthenticator Interface

**Service Name**: `com.opensmsc.SMPPAuthenticator`  
**Object Path**: `/com/opensmsc/SMPPAuthenticator`  
**Interface**: `com.opensmsc.SMPPAuthenticator`

### Methods

#### Authenticate(string username, string password) → (boolean authenticated, uint32 error_code, string message)

Authenticate SMPP client credentials.

**Parameters**:
- `username` (string): SMPP system_id / username
- `password` (string): SMPP password

**Returns**:
- `authenticated` (boolean): True if credentials are valid
- `error_code` (uint32): Error code (0=success)
- `message` (string): Human-readable message

**Error Codes**:
- 0 = Success
- 1 = Invalid username
- 2 = Invalid password
- 3 = Account disabled
- 4 = Service unavailable
- 5 = Internal error

**Usage Example**:
```cpp
// SmppClientHandler authenticates a user
DBusMessage* msg = dbus_message_new_method_call(
    "com.opensmsc.SMPPAuthenticator",
    "/com/opensmsc/SMPPAuthenticator",
    "com.opensmsc.SMPPAuthenticator",
    "Authenticate"
);

const char* user = "test";
const char* pass = "test123";
dbus_message_append_args(msg,
    DBUS_TYPE_STRING, &user,
    DBUS_TYPE_STRING, &pass,
    DBUS_TYPE_INVALID);

DBusMessage* reply = dbus_connection_send_with_reply_and_block(
    conn, msg, 5000, &error);  // 5 second timeout

dbus_bool_t authenticated;
uint32_t error_code;
char* message;
dbus_message_get_args(reply, &error,
    DBUS_TYPE_BOOLEAN, &authenticated,
    DBUS_TYPE_UINT32, &error_code,
    DBUS_TYPE_STRING, &message,
    DBUS_TYPE_INVALID);

if (authenticated) {
    SPDLOG_INFO("Authentication succeeded for user: {}", user);
} else {
    SPDLOG_WARN("Authentication failed: {} (code: {})", message, error_code);
}
```

---

#### ValidateUsername(string username) → (boolean exists)

Check if a username exists in the credential store.

**Parameters**:
- `username`: Username to check

**Returns**:
- `exists` (boolean): True if user exists

---

#### GetAccountInfo(string username) → (string account_id, string status, string account_type)

Get account information for a user.

**Parameters**:
- `username`: Username to query

**Returns**:
- `account_id`: Internal account ID
- `status`: Account status (active, disabled, locked)
- `account_type`: Account type (transmitter, receiver, transceiver)

---

#### ReloadCredentials() → (boolean success, string message)

Reload credentials from configuration file (useful without restarting service).

**Returns**:
- `success`: True if reload succeeded
- `message`: Status message

---

### Signals

#### AuthenticationAttempt(string username, boolean success, string ip_address, uint64 timestamp)

Emitted when an authentication attempt is made (for audit logging).

**Arguments**:
- `username`: Username attempted
- `success`: True if auth succeeded
- `ip_address`: IP address of the attempt
- `timestamp`: Attempt time (microseconds since epoch)

---

#### CredentialsReloaded(uint32 count, string message)

Emitted when credentials are reloaded.

**Arguments**:
- `count`: Number of credentials loaded
- `message`: Status message

---

### Properties

| Property | Type | Access | Description |
|----------|------|--------|-------------|
| `Status` | string | read | Service status: initializing, ready, error |
| `CredentialCount` | uint32 | read | Number of loaded credentials |
| `LastReload` | uint64 | read | Last reload timestamp |
| `ConfigPath` | string | read | Path to credentials config file |

---

## Installation

### D-Bus Service Files

Copy service files to D-Bus service directory:

```bash
# SMPPServer service
sudo cp SMPPServer/files/dbus/com.opensmsc.smppserver.service \
       /usr/share/dbus-1/services/

# Authenticator service
sudo cp Authenticator/files/dbus/com.opensmsc.smppauth.service \
       /usr/share/dbus-1/services/
```

### Interface Definition Files

Copy introspection files to help with D-Bus tools:

```bash
sudo mkdir -p /usr/share/dbus-1/interfaces/

# SMPPServer interface
sudo cp SMPPServer/files/dbus/com.opensmsc.smppserver.xml \
       /usr/share/dbus-1/interfaces/

# Authenticator interface
sudo cp Authenticator/files/dbus/com.opensmsc.smppauth.xml \
       /usr/share/dbus-1/interfaces/
```

### Verify Services

```bash
# List all services (should see new ones)
dbus-send --print-reply --system --dest=org.freedesktop.DBus \
    /org/freedesktop/DBus org.freedesktop.DBus.ListNames

# Check if services are available
busctl list | grep opensmsc
```

---

## Testing D-Bus Methods

### Using dbus-send

Test ClaimSocket (will fail since no real socket):
```bash
dbus-send --print-reply --system \
    --dest=com.opensmsc.SMPPServer \
    /com/opensmsc/SMPPServer \
    com.opensmsc.SMPPServer.ClaimSocket \
    uint16:54321
```

Test Authenticate:
```bash
dbus-send --print-reply --system \
    --dest=com.opensmsc.SMPPAuthenticator \
    /com/opensmsc/SMPPAuthenticator \
    com.opensmsc.SMPPAuthenticator.Authenticate \
    string:"test" string:"test123"
```

### Using busctl

```bash
# List methods and properties
busctl introspect --system com.opensmsc.SMPPServer /com/opensmsc/SMPPServer

# Call a method
busctl call --system com.opensmsc.SMPPAuthenticator /com/opensmsc/SMPPAuthenticator \
    com.opensmsc.SMPPAuthenticator.Authenticate ss test test123

# Read a property
busctl get-property --system com.opensmsc.SMPPServer /com/opensmsc/SMPPServer \
    com.opensmsc.SMPPServer Status
```

### Using Python

```python
import dbus

# Connect to system bus
bus = dbus.SystemBus()

# Get SMPPServer service
server = bus.get_object("com.opensmsc.SMPPServer", "/com/opensmsc/SMPPServer")
smpp_iface = dbus.Interface(server, "com.opensmsc.SMPPServer")

# Get connection count
count = smpp_iface.GetConnectionCount()
print(f"Active connections: {count}")

# Get authenticator service
auth = bus.get_object("com.opensmsc.SMPPAuthenticator", "/com/opensmsc/SMPPAuthenticator")
auth_iface = dbus.Interface(auth, "com.opensmsc.SMPPAuthenticator")

# Test authentication
authenticated, error_code, message = auth_iface.Authenticate("test", "test123")
print(f"Auth result: {authenticated}, Error: {error_code}, Message: {message}")
```

---

## Error Handling

All methods should handle D-Bus errors gracefully:

```cpp
DBusError error;
dbus_error_init(&error);

// Make D-Bus call
// ...

if (dbus_error_is_set(&error)) {
    SPDLOG_ERROR("D-Bus error: {}: {}", error.name, error.message);
    dbus_error_free(&error);
    // Handle error appropriately
}
```

---

## Thread Safety

D-Bus connections should be thread-safe. When calling from multiple threads:

```cpp
// Use a dedicated D-Bus connection per thread, OR
// Protect D-Bus calls with a mutex:

std::mutex dbus_mutex;
{
    std::lock_guard<std::mutex> lock(dbus_mutex);
    // Make D-Bus call
}
```

---

## Monitoring

Monitor D-Bus traffic with:

```bash
# Monitor all D-Bus messages
dbus-monitor --system "interface='com.opensmsc.SMPPServer'"

# Monitor specific service
dbus-monitor --system "interface='com.opensmsc.SMPPAuthenticator'"
```

---

## References

- [D-Bus Specification](https://dbus.freedesktop.org/doc/dbus-daemon.1.html)
- [D-Bus C API](https://dbus.freedesktop.org/doc/api/html/index.html)
- [systemd D-Bus Integration](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html#Type=)

---

**Status**: Complete  
**Last Updated**: 2026-04-20
