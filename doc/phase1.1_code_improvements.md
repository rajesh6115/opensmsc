# Phase 1.1 - Code Improvements & Refactoring

**Created**: 2026-04-19  
**Status**: Planning for Phase 1.1 Implementation  
**Focus**: Code quality, architecture, and best practices

---

## Code Quality Improvements

### 1. Remove Global `g_io_ctx` Variable ⭐

**File**: `SMPPServer/src/main.cpp` (lines 22, 70)

**Current Issue**:
```cpp
// ❌ Global pointer for signal handling
static asio::io_context* g_io_ctx = nullptr;

static void on_signal(int sig) {
    if (g_io_ctx) g_io_ctx->stop();
}

// In main:
asio::io_context io_ctx;
g_io_ctx = &io_ctx;  // Global assignment
std::signal(SIGINT, on_signal);
```

**Problems**:
- ❌ Global variable pollution
- ❌ Not thread-safe (technically)
- ❌ Signal handler outside io_context scope

**Improved Solution**:
```cpp
// ✅ Use ASIO's signal_set instead
asio::io_context io_ctx;
asio::signal_set signals(io_ctx, SIGINT, SIGTERM);

signals.async_wait([&io_ctx](const asio::error_code& ec, int sig) {
    std::cout << "[INFO] Signal " << sig << " — stopping\n";
    io_ctx.stop();
});

// No global needed! Lambda captures io_ctx by reference.
```

**Benefits**:
- ✅ No globals
- ✅ Clean lambda closure
- ✅ Integrated with io_context
- ✅ Thread-safe (ASIO design)
- ✅ Standard ASIO pattern

**Priority**: HIGH (Architecture improvement)

---

## Architecture & Design

### 2. Fix sdbus-c++ API Usage ⭐⭐⭐

**File**: `SMPPServer/src/smpp_service_manager.cpp` (lines 19-21)

**Current Issue**:
```cpp
// ❌ Incorrect API usage
: proxy{sdbus::createProxy(
    sdbus::ServiceName{"org.freedesktop.systemd1"},
    sdbus::ObjectPath{"/org/freedesktop/systemd1"})}
```

**Compilation Errors**:
- `sdbus::ServiceName` not a member of sdbus
- Incorrect unique_ptr initialization

**Solution Needed**:
- Review sdbus-c++ 1.4.0 API documentation
- Use correct types for createProxy()
- Proper initialization pattern

**Priority**: CRITICAL (Blocking compilation)

---

## Testing & Validation

### 3. Add Unit Tests for Core Components ⭐⭐

**Components to Test**:
- [ ] IpValidator (IP whitelist validation)
- [ ] TcpServer (connection handling)
- [ ] SmppServiceManager (SMPP operations)

**Test Framework**: Google Test (already installed)

**Location**: `tests/` folder

**Priority**: HIGH (Code quality)

---

## Documentation

### 4. Update Code Documentation ⭐

**Files**:
- [ ] Add class documentation (Doxygen comments)
- [ ] Document public APIs
- [ ] Add usage examples

**Priority**: MEDIUM (Code maintainability)

---

## Implementation Checklist

### Phase 1.1 Tasks
- [ ] **CRITICAL**: Fix sdbus-c++ API usage (smpp_service_manager.cpp)
- [ ] **HIGH**: Replace global g_io_ctx with asio::signal_set
- [ ] **HIGH**: Implement SMPP BIND operations
- [ ] **HIGH**: Implement SMPP UNBIND operation
- [ ] **HIGH**: Implement SMPP ENQUIRE_LINK (keep-alive)
- [ ] Implement unit tests for core components
- [ ] Update code documentation
- [ ] Verify clean compilation
- [ ] Test with SMPP clients

---

## Code Review Notes

### Design Patterns
- **Signal Handling**: Use ASIO's async signal handling, not C's std::signal
- **Global Variables**: Minimize globals; use RAII and dependency injection
- **D-Bus Integration**: Ensure sdbus-c++ API usage is correct

### Coding Standards
- C++17 features are available
- ASIO for async I/O (already in use)
- Prefer lambdas over free functions where possible
- Use smart pointers (already done with std::make_shared)

---

## Future Improvements (Phase 2+)

- [ ] Add D-Bus service registration for monitoring
- [ ] Implement message persistence (database)
- [ ] Add admin dashboard/UI
- [ ] Performance optimization
- [ ] TLS/SSL support for SMPP

---

## References

**ASIO Signal Handling**:
- [ASIO signal_set documentation](https://think-async.com/Asio/asio-1.24.0/doc/asio/reference/signal_set.html)
- Pattern: async_wait + lambda

**sdbus-c++ API**:
- [sdbus-c++ GitHub](https://github.com/Kistler-Group/sdbus-cpp)
- Example: Correct createProxy() usage

**SMPP Protocol**:
- [SMPP v3.4 Specification](https://en.wikipedia.org/wiki/Short_Message_Peer-to-Peer)
- Operations: BIND, UNBIND, ENQUIRE_LINK, SUBMIT_SM, DELIVER_SM

---

**Status**: Ready for Phase 1.1 Implementation

Next: Start implementing improvements and SMPP protocol operations.
