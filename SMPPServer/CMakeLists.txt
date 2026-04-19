cmake_minimum_required(VERSION 3.16)
project(simple_smpp_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find packages using pkg-config ────────────────────────────────────────────

find_package(PkgConfig REQUIRED)

# ASIO (standalone, header-only)
# Install: apt install libasio-dev (Debian/Ubuntu)
# Check for asio.hpp header (pkg-config not always available for libasio-dev)
find_path(ASIO_INCLUDE_DIR asio.hpp
    HINTS /usr/include /usr/local/include
    DOC "ASIO header directory"
)

if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "asio.hpp not found. Please install libasio-dev package.")
endif()

message(STATUS "Found ASIO headers at: ${ASIO_INCLUDE_DIR}")

# sdbus-c++ (modern C++ D-Bus binding)
# Install: apt install libsdbus-c++-dev (Debian/Ubuntu)
# This is MANDATORY - build will fail if not found
pkg_check_modules(SDBUS_CPP REQUIRED sdbus-c++)

# spdlog (fast C++ logging)
# Install: apt install libspdlog-dev (Debian/Ubuntu)
pkg_check_modules(SPDLOG QUIET spdlog)
if(NOT SPDLOG_FOUND)
    # Fallback: find headers directly
    find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h
        HINTS /usr/include /usr/local/include
        DOC "spdlog header directory"
    )
    if(NOT SPDLOG_INCLUDE_DIR)
        message(FATAL_ERROR "spdlog not found. Please install libspdlog-dev package.")
    endif()
else()
    set(SPDLOG_INCLUDE_DIR ${SPDLOG_INCLUDEDIR})
endif()

# libfmt (formatting library required by spdlog)
# Install: apt install libfmt-dev (Debian/Ubuntu)
pkg_check_modules(FMT QUIET fmt)
if(NOT FMT_FOUND)
    find_library(FMT_LIBRARY fmt)
    if(NOT FMT_LIBRARY)
        message(WARNING "libfmt not found, spdlog may not link properly")
    endif()
endif()

# ── Create interface library for ASIO ─────────────────────────────────────────
add_library(asio_iface INTERFACE)
target_include_directories(asio_iface INTERFACE
    ${ASIO_INCLUDE_DIR}
)
target_compile_definitions(asio_iface INTERFACE
    ASIO_STANDALONE          # No Boost dependency
    ASIO_NO_DEPRECATED
)


# ── Sources ──────────────────────────────────────────────────────────────────

# Find all smppcxx source files
file(GLOB SMPPCXX_SOURCES
    ../external/smppcxx/src/*.cpp
)

set(SOURCES
    src/main.cpp
    src/tcp_server.cpp
    src/ip_validator.cpp
    src/smpp_service_manager.cpp
    src/smpp_session.cpp
    src/smpp_handler.cpp
    ${SMPPCXX_SOURCES}
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ../external/smppcxx/include
    ${SPDLOG_INCLUDE_DIR}
)

# Link libraries using pkg-config imported targets
target_link_libraries(${PROJECT_NAME} PRIVATE
    asio_iface
    ${SDBUS_CPP_LIBRARIES}
    ${FMT_LIBRARIES}
    pthread
)

# Add include directories from pkg-config
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SDBUS_CPP_INCLUDE_DIRS}
)

# Add compile flags from pkg-config
target_compile_options(${PROJECT_NAME} PRIVATE
    ${SDBUS_CPP_CFLAGS}
    -Wall -Wextra -Wpedantic
)

# Add link flags from pkg-config
target_link_options(${PROJECT_NAME} PRIVATE
    ${SDBUS_CPP_LDFLAGS}
)

# ── Install ──────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES config/allowed_ips.conf
        DESTINATION /etc/simple_smpp_server)
install(FILES systemd/SMPPAuthenticator@.service
        DESTINATION /etc/systemd/system)

message(STATUS "")
message(STATUS "  ═══════════════════════════════════════════════════════")
message(STATUS "  simple_smpp_server ${PROJECT_VERSION}")
message(STATUS "  ───────────────────────────────────────────────────────")
message(STATUS "  ASIO (standalone) : ${ASIO_INCLUDE_DIR}")
message(STATUS "  sdbus-c++ (D-Bus) : ${SDBUS_CPP_VERSION}")
message(STATUS "  spdlog            : ${SPDLOG_INCLUDE_DIR} (header-only)")
message(STATUS "  C++ Standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler          : ${CMAKE_CXX_COMPILER}")
message(STATUS "  ═══════════════════════════════════════════════════════")
message(STATUS "")
