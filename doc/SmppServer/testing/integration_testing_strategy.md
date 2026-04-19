# Integration Testing Strategy

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: How to test class interactions  

---

## Integration Test Strategy

Integration tests verify multiple classes working together. They:
- Run in Docker (server process)
- Connect via TCP from HOST (Python smpplib client)
- Test realistic workflows

### Setup

```
Host (macOS/Linux/Windows)
  └─ Python smpplib test script (tests/quick_test.py)
     └─ TCP connects to 127.0.0.1:2775
     
Docker Container (smsc-dev-container)
  └─ SMPPServer binary
     └─ Listening on 0.0.0.0:2775
     └─ Logs to /var/log/simple_smpp_server/server.log
```

### Test File Organization

```
tests/
├── integration/
│   ├── test_smpp_connection.cpp          # SmppConnection + ASIO socket
│   ├── test_tcp_server.cpp               # TcpServer + SmppConnection
│   └── test_smpp_server.cpp              # Full server startup/shutdown
├── e2e/
│   └── test_bind_flow.py                 # Python client connects & BINDs
├── quick_test.py                         # Main integration test script
└── fixtures/
    └── whitelist.txt                     # Test IP whitelist config
```

---

## Python Integration Test (E2E)

Use `smpplib` Python library to connect and test SMPP flows.

### Installation

```bash
pip install smpplib
```

### Test Script Template

```python
#!/usr/bin/env python3
"""
Integration test for SMPPServer.

Runs from HOST, connects to server running in Docker at 127.0.0.1:2775.
"""

import smpplib
import smpplib.client
import time
import sys

def test_bind_transmitter():
    """Test successful BIND_TRANSMITTER"""
    client = smpplib.client.Client('127.0.0.1', 2775, timeout=10)
    
    try:
        # Connect
        client.connect()
        print("✓ Connection established")
        
        # Bind
        client.bind(smpplib.consts.SMPP_CLIENT_TX, system_id='test', password='pass')
        print("✓ BIND_TRANSMITTER successful")
        
        # Send ENQUIRE_LINK (keep-alive)
        client.enquire_link()
        print("✓ ENQUIRE_LINK successful")
        
        # Unbind
        client.unbind()
        print("✓ UNBIND successful")
        
        client.disconnect()
        return True
        
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        client.disconnect()

def test_bind_receiver():
    """Test successful BIND_RECEIVER"""
    client = smpplib.client.Client('127.0.0.1', 2775, timeout=10)
    
    try:
        client.connect()
        client.bind(smpplib.consts.SMPP_CLIENT_RX, system_id='test', password='pass')
        client.unbind()
        client.disconnect()
        return True
    except Exception as e:
        print(f"✗ BIND_RECEIVER failed: {e}")
        return False

def test_invalid_credentials():
    """Test BIND with invalid credentials"""
    client = smpplib.client.Client('127.0.0.1', 2775, timeout=10)
    
    try:
        client.connect()
        client.bind(smpplib.consts.SMPP_CLIENT_TX, system_id='test', password='wrong')
        print("✗ Should have failed with wrong password")
        return False
    except smpplib.exception.SMPPException as e:
        if "Invalid" in str(e) or "password" in str(e).lower():
            print(f"✓ Correctly rejected invalid credentials: {e}")
            return True
        else:
            print(f"✗ Unexpected error: {e}")
            return False
    finally:
        client.disconnect()

if __name__ == '__main__':
    results = []
    
    print("Running integration tests...")
    print()
    
    results.append(("BIND_TRANSMITTER", test_bind_transmitter()))
    print()
    
    results.append(("BIND_RECEIVER", test_bind_receiver()))
    print()
    
    results.append(("Invalid credentials", test_invalid_credentials()))
    print()
    
    # Summary
    passed = sum(1 for _, r in results if r)
    total = len(results)
    
    print(f"Results: {passed}/{total} tests passed")
    
    for name, result in results:
        status = "✓" if result else "✗"
        print(f"  {status} {name}")
    
    sys.exit(0 if passed == total else 1)
```

### Running from HOST

```bash
# Terminal 1: Start server in Docker
docker exec -it smsc-dev-container /workspace/build/SMPPServer/simple_smpp_server

# Terminal 2: Run Python tests from host
cd /workspace
python3 tests/quick_test.py

# Expected output
# ✓ Connection established
# ✓ BIND_TRANSMITTER successful
# ✓ ENQUIRE_LINK successful
# ✓ UNBIND successful
```

---

## C++ Integration Tests

Test class interactions using real (not mocked) dependencies.

### SmppConnection Integration Test

```cpp
// tests/integration/test_smpp_connection.cpp
#include <catch2/catch_test_macros.hpp>
#include <asio.hpp>
#include "smpp_connection.hpp"
#include "smpp_session.hpp"
#include "smpp_message_processor.hpp"
#include "mocks/mock_logger.hpp"
#include "mocks/mock_session_manager.hpp"

namespace smpp {

TEST_CASE("SmppConnection", "[integration][smpp_connection]") {
    
    SECTION("Receives complete PDU and dispatches") {
        // Create mock processor
        auto processor = std::make_shared<MockSmppMessageProcessor>();
        auto session = std::make_shared<SmppSession>("sess_1", "127.0.0.1");
        auto logger = std::make_shared<MockLogger>();
        auto session_manager = std::make_shared<MockSessionManager>();
        
        // Setup processor to return success response
        processor->set_response(SmppMessageEncoder::build_bind_transmitter_resp(
            ESME_ROK,
            1,
            "TestServer"));
        
        // Create connection with mock socket
        asio::io_context io;
        asio::ip::tcp::socket socket(io);
        
        auto connection = std::make_shared<SmppConnection>(
            std::move(socket),
            session,
            processor,
            session_manager,
            logger);
        
        // Simulate receiving a complete BIND_TRANSMITTER PDU
        std::vector<uint8_t> pdu = SmppMessage(
            BIND_TRANSMITTER,
            0,
            1,
            std::vector<uint8_t>{ 't', 'e', 's', 't', '\0', 'p', 'a', 's', 's', '\0' }
        ).encode();
        
        // In real test, would feed bytes to connection's buffers
        // For now, just verify structure exists
        REQUIRE(pdu.size() > 16);
        REQUIRE(session->state() == SmppSession::State::UNBOUND);
    }
}

}  // namespace smpp
```

### TcpServer Integration Test

```cpp
// tests/integration/test_tcp_server.cpp
#include <catch2/catch_test_macros.hpp>
#include "tcp_server.hpp"
#include "smpp_message_processor.hpp"
#include "mocks/mock_ip_validator.hpp"

namespace smpp {

TEST_CASE("TcpServer", "[integration][tcp_server]") {
    
    SECTION("Accepts connections from whitelisted IPs") {
        asio::io_context io;
        
        auto ip_validator = std::make_shared<MockIpValidator>();
        ip_validator->whitelist("127.0.0.1");
        
        auto processor = std::make_shared<MockSmppMessageProcessor>();
        auto session_manager = std::make_shared<SessionManager>();
        auto logger = std::make_shared<MockLogger>();
        
        auto server = std::make_shared<TcpServer>(
            io,
            2775,
            ip_validator,
            processor,
            session_manager,
            logger);
        
        server->start();
        
        // In real test, would connect from another thread
        // and verify connection accepted
        REQUIRE(server->active_connection_count() == 0);  // Before connect
    }
}

}  // namespace smpp
```

---

## Test Execution Workflow

### 1. Build and Start Server

```bash
# In host terminal
cd /workspace

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Start server in Docker
docker exec -d smsc-dev-container \
  /workspace/build/SMPPServer/simple_smpp_server \
  --log-level=debug \
  --port=2775
```

### 2. Run Unit Tests (Optional, can run before or after server)

```bash
# Unit tests don't need running server
ctest --build-dir build -R "test_smpp" --output-on-failure
```

### 3. Run Integration Tests

```bash
# Python integration tests (requires running server)
python3 tests/quick_test.py

# Check server logs
docker exec smsc-dev-container tail -f /var/log/simple_smpp_server/server.log
```

### 4. Stop Server

```bash
docker exec smsc-dev-container pkill -f simple_smpp_server
```

---

## Continuous Integration (CI)

In `Build_and_Test_Plan.md`, the CI pipeline:

1. **Build**: `cmake --build build --parallel`
2. **Unit tests**: `ctest --build-dir build --output-on-failure` (all unit tests)
3. **Start server**: `docker exec -d smsc-dev-container simple_smpp_server`
4. **Integration tests**: `python3 tests/quick_test.py`
5. **Logs**: Capture `/var/log/simple_smpp_server/server.log` as artifact
6. **Stop**: `docker exec smsc-dev-container pkill -f simple_smpp_server`

---

## Troubleshooting Integration Tests

### Server not responding

```bash
# Check if server is running
docker exec smsc-dev-container ps aux | grep simple_smpp_server

# Check logs
docker exec smsc-dev-container tail -20 /var/log/simple_smpp_server/server.log

# Check port binding
docker exec smsc-dev-container netstat -tlnp | grep 2775
```

### Python smpplib connection timeout

```bash
# Increase timeout in test
client = smpplib.client.Client('127.0.0.1', 2775, timeout=30)

# Or check if port is accessible from host
nc -zv 127.0.0.1 2775
```

### Incomplete PDU

```bash
# If server crashes on receiving data, check:
# 1. SmppMessageParser handles partial messages
# 2. ASIO async callbacks don't block on reads
# 3. Buffer management in on_body_read
```

---

**Next**: test_cases.md
