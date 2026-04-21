cmake_minimum_required(VERSION 3.16)
project(simple_smpp_server
    VERSION 0.1.0
    DESCRIPTION "SMPP Server - Process-per-client microservices"
    HOMEPAGE_URL "https://github.com/rajesh6115/opensmsc"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Generate config.h from config.h.in ───────────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config/config.h"
    @ONLY
)

# ── Find packages using pkg-config ────────────────────────────────────────────

find_package(PkgConfig REQUIRED)

# ASIO is configured in root CMakeLists.txt as asio_iface interface library

# sdbus-c++ (modern C++ D-Bus binding)
# Install: apt install libsdbus-c++-dev (Debian/Ubuntu)
# This is MANDATORY - build will fail if not found
pkg_check_modules(SDBUS_CPP REQUIRED sdbus-c++)

# Find sdbus-c++-xml2cpp tool for generating adaptors from D-Bus XML files
find_program(SDBUS_CPP_XML2CPP sdbus-c++-xml2cpp
    HINTS /usr/bin /usr/local/bin
    DOC "sdbus-c++-xml2cpp tool for generating D-Bus adaptors"
)

if(NOT SDBUS_CPP_XML2CPP)
    message(FATAL_ERROR "sdbus-c++-xml2cpp tool not found. Please install libsdbus-c++-dev package.")
endif()

# Set generated files directory (source tree for IDE support)
set(GENERATED_DBUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

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

# ── Generate D-Bus Adaptors from XML ────────────────────────────────────────

# Generate SmppServer adaptor from D-Bus interface definition
set(DBUS_XML_FILE "${CMAKE_CURRENT_SOURCE_DIR}/files/dbus/com.opensmsc.SmppServer.xml")
set(SMPP_SERVER_ADAPTOR_HEADER "${GENERATED_DBUS_DIR}/SmppServerAdaptor.gen.h")

add_custom_command(
    OUTPUT "${SMPP_SERVER_ADAPTOR_HEADER}"
    COMMAND "${SDBUS_CPP_XML2CPP}" "${DBUS_XML_FILE}"
            --adaptor="${SMPP_SERVER_ADAPTOR_HEADER}"
    DEPENDS "${DBUS_XML_FILE}"
    COMMENT "Generating D-Bus adaptor for SmppServer from XML"
    VERBATIM
)

# Create a custom target to ensure the adaptor is generated before compilation
add_custom_target(generate_smpp_server_adaptor ALL
    DEPENDS "${SMPP_SERVER_ADAPTOR_HEADER}"
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
    src/smpp_pdu_framer.cpp
    ${SMPPCXX_SOURCES}
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Ensure adaptor is generated before building the executable
add_dependencies(${PROJECT_NAME} generate_smpp_server_adaptor)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    "${CMAKE_CURRENT_BINARY_DIR}/config"
    "${GENERATED_DBUS_DIR}"
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

# Install executable
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Install configuration files
install(FILES config/allowed_ips.conf
        DESTINATION /etc/simple_smpp_server)

# Install systemd service files
install(FILES systemd/SMPPAuthenticator@.service
        DESTINATION /etc/systemd/system)

# Install D-Bus interface definition
# This allows other services/modules to generate proxy/client code from the interface
install(FILES files/dbus/com.opensmsc.SmppServer.xml
        DESTINATION /usr/share/dbus-1/interfaces/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

message(STATUS "")
message(STATUS "  ═══════════════════════════════════════════════════════")
message(STATUS "  simple_smpp_server ${PROJECT_VERSION}")
message(STATUS "  ───────────────────────────────────────────────────────")
message(STATUS "  ASIO (standalone) : ${ASIO_INCLUDE_DIR}")
message(STATUS "  sdbus-c++ (D-Bus) : ${SDBUS_CPP_VERSION}")
message(STATUS "  sdbus-c++ xml2cpp : ${SDBUS_CPP_XML2CPP}")
message(STATUS "  Generated DBus    : ${GENERATED_DBUS_DIR}")
message(STATUS "  D-Bus Interface   : /usr/share/dbus-1/interfaces/com.opensmsc.SmppServer.xml")
message(STATUS "  spdlog            : ${SPDLOG_INCLUDE_DIR} (header-only)")
message(STATUS "  C++ Standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler          : ${CMAKE_CXX_COMPILER}")
message(STATUS "  ═══════════════════════════════════════════════════════")
message(STATUS "")
