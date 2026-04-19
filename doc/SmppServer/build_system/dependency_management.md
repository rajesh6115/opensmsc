# Dependency Management

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: Third-party library integration and versions  

---

## Dependencies

### Direct Dependencies (In Code)

| Library | Version | Purpose | Type | License |
|---------|---------|---------|------|---------|
| **ASIO** | 1.24+ | Async I/O, networking | System | BSL-1.0 |
| **spdlog** | 1.14.1 | Logging | FetchContent | MIT |
| **sdbus-c++** | Latest | D-Bus C++ binding | System | LGPL-2.1 |
| **Catch2** | 3.5.0 | Unit testing | FetchContent | BSL-1.0 |

### Transitive Dependencies

- **OpenSSL** (via sdbus-c++ if needed): TLS/cryptography
- **libdbus** (via sdbus-c++): D-Bus implementation

---

## ASIO Setup

### Include

```cpp
#include <asio.hpp>
```

### Version Check

```cpp
// In code, check version if needed:
static_assert(ASIO_VERSION >= 102400);  // 1.24.0
```

### Ubuntu/Debian Installation

```bash
sudo apt-get install libasio-dev
```

### macOS Installation

```bash
brew install asio
```

### Windows (Header-Only)

ASIO can be used header-only. Add include path:

```cmake
# CMakeLists.txt
find_package(asio REQUIRED)
target_link_libraries(... asio::asio)
```

---

## spdlog (Logging)

### FetchContent Integration

Already configured in root CMakeLists.txt:

```cmake
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(spdlog)

target_link_libraries(smpp_server_lib PUBLIC spdlog::spdlog_header_only)
```

### Why FetchContent?

- **Reproducible**: Same version for all builds
- **No system dependency**: Works on any OS
- **Header-only**: No compilation, fast builds

### Version Selection

Current: **v1.14.1** (latest stable as of 2026-04-19)

Upgrade path:
- Check releases: https://github.com/gabime/spdlog/releases
- Update GIT_TAG in CMakeLists.txt
- Rebuild

---

## sdbus-c++ (D-Bus)

### Ubuntu/Debian Installation

```bash
sudo apt-get install libsdbus-c++-dev
```

### CMake Integration

```cmake
find_package(sdbus-c++ REQUIRED)
target_link_libraries(smpp_server_lib PUBLIC SDBusCpp::sdbus-c++)
```

### Docker Integration

Already in Dockerfile.dev:

```dockerfile
RUN apt-get update && apt-get install -y libsdbus-c++-dev
```

### D-Bus Connection

```cpp
// In DBusAuthenticator
auto connection = sdbus::createSessionBusConnection();  // Testing
auto connection = sdbus::createSystemBusConnection();   // Production
```

### D-Bus Service (SMPPAuthenticator)

Separate service; not part of SMPPServer build. Uses interface:

```
/com/telecom/SMPPAuthenticator
  Method: Authenticate(username:s, password:s, client_ip:s) → (success:b, message:s)
```

---

## Catch2 (Testing)

### FetchContent Integration

```cmake
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

target_link_libraries(test_executable Catch2::Catch2WithMain)
```

### Test Macro

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("ClassName", "[class_name]") {
    SECTION("feature") {
        REQUIRE(condition);
    }
}
```

### Running Tests

```bash
# All tests
ctest --build-dir build --output-on-failure

# Specific test
build/tests/unit/test_smpp_message

# Catch2 command-line options
build/tests/unit/test_smpp_message -h  # Help
build/tests/unit/test_smpp_message --list-tests
```

---

## Compiler Requirements

- **C++17** or newer
- **GCC 7+** or **Clang 5+** or **MSVC 2017+**

### Docker Compiler

```dockerfile
# In Dockerfile.dev
FROM ubuntu:22.04
RUN apt-get install -y g++-11  # C++17 ready
```

### Verify Compiler

```bash
g++ --version
# g++ (Ubuntu 11.4.0-1ubuntu1~22.04.1) 11.4.0
# Supports C++17 ✓
```

---

## Build System Requirements

- **CMake 3.20+** (for FetchContent)
- **Make** or **Ninja** (build tool)

### Check CMake

```bash
cmake --version
# cmake version 3.22.1
```

---

## Docker Layer Dependencies

Full Dockerfile.dev dependency layer:

```dockerfile
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    libsdbus-c++-dev \
    && rm -rf /var/lib/apt/lists/*
```

---

## Upgrading Dependencies

### Safer Approach

1. Test upgrade in feature branch
2. Run full test suite
3. Check compatibility notes
4. Commit together with CHANGELOG entry

### ASIO Upgrade

```cmake
# Update find_package or FetchContent
find_package(asio 1.28.0 REQUIRED)  # Example: upgrade to 1.28
```

### spdlog Upgrade

```cmake
GIT_TAG v1.15.0  # Example: next version
```

### Catch2 Upgrade

```cmake
GIT_TAG v3.6.0  # Example
```

### sdbus-c++ Upgrade

```bash
# On Docker build
RUN apt-get install libsdbus-c++-dev=<new-version>
```

---

## License Compatibility

All dependencies are OSI-approved open source:

| Library | License | Commercial Use | Modification | Redistribution |
|---------|---------|---|---|---|
| ASIO | BSL-1.0 | ✓ Yes | ✓ Yes | ✓ Yes |
| spdlog | MIT | ✓ Yes | ✓ Yes | ✓ Yes |
| sdbus-c++ | LGPL-2.1 | ✓ Yes | ✓ Yes | ✓ Yes (with notice) |
| Catch2 | BSL-1.0 | ✓ Yes | ✓ Yes | ✓ Yes |

**Note**: LGPL requires distributing source or object files if modified. Linking is OK.

---

## Troubleshooting

### Missing ASIO Headers

```bash
# Ubuntu
sudo apt-get install libasio-dev

# Or download from: https://github.com/asio/asio
```

### Missing sdbus-c++

```bash
# Ubuntu
sudo apt-get install libsdbus-c++-dev

# Check installation
find /usr -name "sdbus*" -type f
```

### CMake Can't Find Package

```bash
# In CMakeLists.txt, add debug
message(STATUS "sdbus-c++ found: ${sdbus-c++_FOUND}")
message(STATUS "ASIO found: ${asio_FOUND}")

# Or set explicit path
set(sdbus-c++_DIR /usr/lib/cmake/sdbus-c++)
```

### Catch2 Not Found

FetchContent will download automatically. If offline:

```bash
# Pre-download and use local path
cmake -DFETCHCONTENT_SOURCE_DIR_CATCH2=/path/to/catch2 ...
```

---

**Next**: error_handling documents
