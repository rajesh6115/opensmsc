# CMake Configuration

**Date**: 2026-04-19  
**Version**: 1.0  
**Purpose**: CMakeLists.txt setup for building SMPPServer  

---

## Project Structure

```
SMPPServer/
├── CMakeLists.txt                    [Root, see below]
├── include/
│   ├── smpp_message.hpp
│   ├── smpp_session.hpp
│   ├── smpp_message_parser.hpp
│   ├── smpp_message_encoder.hpp
│   ├── tcp_server.hpp
│   ├── smpp_connection.hpp
│   ├── smpp_server.hpp
│   ├── smpp_message_processor.hpp
│   ├── smpp_request_handler.hpp
│   ├── handlers/
│   │   ├── bind_transmitter_handler.hpp
│   │   ├── bind_receiver_handler.hpp
│   │   ├── bind_transceiver_handler.hpp
│   │   ├── unbind_handler.hpp
│   │   └── enquire_link_handler.hpp
│   ├── validators/
│   │   ├── ip_validator.hpp
│   │   └── credential_validator.hpp
│   ├── authenticators/
│   │   └── dbus_authenticator.hpp
│   ├── session_manager.hpp
│   └── logger.hpp
├── src/
│   ├── main.cpp
│   ├── smpp_message.cpp
│   ├── smpp_session.cpp
│   ├── smpp_message_parser.cpp
│   ├── smpp_message_encoder.cpp
│   ├── tcp_server.cpp
│   ├── smpp_connection.cpp
│   ├── smpp_server.cpp
│   ├── smpp_message_processor.cpp
│   ├── handlers/
│   │   ├── bind_transmitter_handler.cpp
│   │   ├── bind_receiver_handler.cpp
│   │   ├── bind_transceiver_handler.cpp
│   │   ├── unbind_handler.cpp
│   │   └── enquire_link_handler.cpp
│   ├── validators/
│   │   ├── ip_validator.cpp
│   │   └── credential_validator.cpp
│   ├── authenticators/
│   │   └── dbus_authenticator.cpp
│   ├── session_manager.cpp
│   └── logger.cpp
└── tests/
    ├── CMakeLists.txt                [Testing setup]
    ├── unit/
    │   ├── test_smpp_message.cpp
    │   ├── test_smpp_session.cpp
    │   └── [11 more unit tests]
    ├── integration/
    │   ├── test_smpp_connection.cpp
    │   ├── test_tcp_server.cpp
    │   └── test_smpp_server.cpp
    ├── e2e/
    │   └── test_bind_flow.py
    ├── quick_test.py
    └── mocks/
        ├── mock_dbus_authenticator.hpp
        └── [4 more mocks]
```

---

## Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(simple_smpp_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# ============================================================================
# FetchContent for dependencies (reproducible builds)
# ============================================================================

include(FetchContent)

# spdlog: Logging library (header-only)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE)

# Catch2: Testing framework
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(spdlog Catch2)

# ============================================================================
# External packages (system-installed)
# ============================================================================

find_package(asio REQUIRED)
find_package(sdbus-c++ REQUIRED)
find_package(Threads REQUIRED)

# ============================================================================
# SMPPServer Library
# ============================================================================

add_library(smpp_server_lib STATIC
    # Protocol layer
    src/smpp_message.cpp
    src/smpp_message_parser.cpp
    src/smpp_message_encoder.cpp
    
    # Business logic layer
    src/smpp_session.cpp
    src/smpp_message_processor.cpp
    
    # Handler implementations
    src/handlers/bind_transmitter_handler.cpp
    src/handlers/bind_receiver_handler.cpp
    src/handlers/bind_transceiver_handler.cpp
    src/handlers/unbind_handler.cpp
    src/handlers/enquire_link_handler.cpp
    
    # Infrastructure layer
    src/validators/ip_validator.cpp
    src/validators/credential_validator.cpp
    src/authenticators/dbus_authenticator.cpp
    src/logger.cpp
    src/session_manager.cpp
    
    # Presentation layer
    src/tcp_server.cpp
    src/smpp_connection.cpp
    src/smpp_server.cpp
)

target_include_directories(smpp_server_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(smpp_server_lib PUBLIC
    spdlog::spdlog_header_only
    asio::asio
    SDBusCpp::sdbus-c++
    Threads::Threads
)

# ============================================================================
# SMPPServer Executable
# ============================================================================

add_executable(simple_smpp_server
    src/main.cpp
)

target_link_libraries(simple_smpp_server PRIVATE
    smpp_server_lib
)

# ============================================================================
# Installation
# ============================================================================

install(TARGETS simple_smpp_server
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/smpp
)

# ============================================================================
# Testing
# ============================================================================

enable_testing()
add_subdirectory(tests)
```

---

## tests/CMakeLists.txt

```cmake
# ============================================================================
# Unit Tests
# ============================================================================

# Macro to add unit test
macro(add_unit_test test_name test_file)
    add_executable(${test_name} unit/${test_file})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        smpp_server_lib
        Catch2::Catch2WithMain
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro()

add_unit_test(test_smpp_message test_smpp_message.cpp)
add_unit_test(test_smpp_session test_smpp_session.cpp)
add_unit_test(test_smpp_message_parser test_smpp_message_parser.cpp)
add_unit_test(test_smpp_message_encoder test_smpp_message_encoder.cpp)
add_unit_test(test_ip_validator test_ip_validator.cpp)
add_unit_test(test_credential_validator test_credential_validator.cpp)
add_unit_test(test_dbus_authenticator test_dbus_authenticator.cpp)
add_unit_test(test_logger test_logger.cpp)
add_unit_test(test_session_manager test_session_manager.cpp)
add_unit_test(test_bind_transmitter_handler test_bind_transmitter_handler.cpp)
add_unit_test(test_bind_receiver_handler test_bind_receiver_handler.cpp)
add_unit_test(test_bind_transceiver_handler test_bind_transceiver_handler.cpp)
add_unit_test(test_unbind_handler test_unbind_handler.cpp)
add_unit_test(test_enquire_link_handler test_enquire_link_handler.cpp)
add_unit_test(test_smpp_message_processor test_smpp_message_processor.cpp)

# ============================================================================
# Integration Tests
# ============================================================================

macro(add_integration_test test_name test_file)
    add_executable(${test_name} integration/${test_file})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        smpp_server_lib
        Catch2::Catch2WithMain
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro()

add_integration_test(test_smpp_connection integration/test_smpp_connection.cpp)
add_integration_test(test_tcp_server integration/test_tcp_server.cpp)
add_integration_test(test_smpp_server integration/test_smpp_server.cpp)

# ============================================================================
# E2E Tests (Python, run separately)
# ============================================================================

# E2E tests are Python scripts; run with:
# python3 tests/quick_test.py
# (Server must be running first)
```

---

## Build Commands

### Development Build (with debug symbols)

```bash
cd /workspace
cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -g"

cmake --build build --parallel $(nproc)
```

### Release Build (optimized)

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -O3"

cmake --build build --parallel $(nproc)
```

### With Coverage

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage"

cmake --build build
ctest --build-dir build
lcov --capture --directory build --output-file coverage.info
```

---

## Common CMake Targets

```bash
# Build everything
cmake --build build

# Build specific target
cmake --build build --target simple_smpp_server
cmake --build build --target smpp_server_lib

# Run all tests
ctest --build-dir build --output-on-failure

# Run specific test
ctest --build-dir build -R test_smpp_message --output-on-failure

# Install binaries
cmake --install build --prefix /usr/local
```

---

## Compiler Warnings

Compiled with `-Wall -Wextra -Wpedantic` to catch issues early. Fix all warnings before commit:

```bash
# Build and show all warnings
cmake --build build -- VERBOSE

# Common warnings to fix:
# - unused variable: remove or use [[maybe_unused]]
# - deprecated function: use alternative
# - shadowed variable: rename one of them
# - conversion from larger to smaller type: use explicit cast
```

---

**Next**: dependency_management.md
