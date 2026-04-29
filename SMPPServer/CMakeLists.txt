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

# ASIO standalone (header-only)
if(NOT TARGET asio_iface)
    find_path(ASIO_INCLUDE_DIR asio.hpp
        HINTS /usr/include /usr/local/include
        DOC "ASIO standalone include directory"
    )
    if(NOT ASIO_INCLUDE_DIR)
        message(FATAL_ERROR "ASIO not found. Install libasio-dev.")
    endif()
    add_library(asio_iface INTERFACE)
    target_include_directories(asio_iface INTERFACE ${ASIO_INCLUDE_DIR})
    target_compile_definitions(asio_iface INTERFACE ASIO_STANDALONE)
endif()

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

# Generate IServer adaptor + proxy from IServer.xml
set(ISERVER_XML "${CMAKE_CURRENT_SOURCE_DIR}/files/dbus/IServer.xml")
set(ISERVER_ADAPTOR "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/IServer_adaptor.hpp")
set(ISERVER_PROXY   "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/IServer_proxy.hpp")

add_custom_command(
    OUTPUT  "${ISERVER_ADAPTOR}" "${ISERVER_PROXY}"
    COMMAND "${SDBUS_CPP_XML2CPP}" "${ISERVER_XML}"
            --adaptor="${ISERVER_ADAPTOR}"
            --proxy="${ISERVER_PROXY}"
    DEPENDS "${ISERVER_XML}"
    COMMENT "Generating IServer adaptor/proxy from IServer.xml"
    VERBATIM
)

add_custom_target(generate_iserver_dbus ALL
    DEPENDS "${ISERVER_ADAPTOR}" "${ISERVER_PROXY}"
)

# Install IServer.xml to standard D-Bus interfaces path
install(FILES "${ISERVER_XML}"
        DESTINATION /usr/share/dbus-1/interfaces/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

# ── Sources ──────────────────────────────────────────────────────────────────

set(SOURCES
    src/main.cpp
    src/tcp_server.cpp
    src/ip_validator.cpp
    src/connection_registry.cpp
    src/smpp_server_service.cpp
    src/session_launcher.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Ensure adaptors are generated before building the executable
add_dependencies(${PROJECT_NAME} generate_iserver_dbus)

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

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
find_package(GTest REQUIRED)

add_executable(test_iserver_dbus tests/test_iserver_dbus.cpp)
add_dependencies(test_iserver_dbus generate_iserver_dbus)

target_include_directories(test_iserver_dbus PRIVATE
    include
    ${SDBUS_CPP_INCLUDE_DIRS}
)
target_link_libraries(test_iserver_dbus PRIVATE
    GTest::gtest
    GTest::gtest_main
    ${SDBUS_CPP_LIBRARIES}
)
target_compile_options(test_iserver_dbus PRIVATE ${SDBUS_CPP_CFLAGS})
target_link_options(test_iserver_dbus PRIVATE ${SDBUS_CPP_LDFLAGS})

add_test(NAME IServerDbusTests COMMAND test_iserver_dbus)

add_executable(test_connection_registry tests/test_connection_registry.cpp src/connection_registry.cpp)
target_include_directories(test_connection_registry PRIVATE include)
target_link_libraries(test_connection_registry PRIVATE GTest::gtest GTest::gtest_main)
target_compile_options(test_connection_registry PRIVATE -Wall -Wextra)
add_test(NAME ConnectionRegistryTests COMMAND test_connection_registry)

add_executable(test_session_launcher tests/test_session_launcher.cpp src/session_launcher.cpp)
target_include_directories(test_session_launcher PRIVATE include ${SPDLOG_INCLUDE_DIR})
target_link_libraries(test_session_launcher PRIVATE GTest::gtest GTest::gtest_main ${FMT_LIBRARIES} pthread)
target_compile_options(test_session_launcher PRIVATE -Wall -Wextra)
add_test(NAME SessionLauncherTests COMMAND test_session_launcher)

add_executable(test_iserver_methods
    tests/test_iserver_methods.cpp
    src/smpp_server_service.cpp
    src/connection_registry.cpp
)
add_dependencies(test_iserver_methods generate_iserver_dbus)
target_include_directories(test_iserver_methods PRIVATE include ${SDBUS_CPP_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIR})
target_link_libraries(test_iserver_methods PRIVATE GTest::gtest GTest::gtest_main ${SDBUS_CPP_LIBRARIES} ${FMT_LIBRARIES} pthread)
target_compile_options(test_iserver_methods PRIVATE ${SDBUS_CPP_CFLAGS} -Wall -Wextra)
target_link_options(test_iserver_methods PRIVATE ${SDBUS_CPP_LDFLAGS})
add_test(NAME IServerMethodsTests COMMAND test_iserver_methods)

add_executable(test_route_message
    tests/test_route_message.cpp
    src/smpp_server_service.cpp
    src/connection_registry.cpp
)
add_dependencies(test_route_message generate_iserver_dbus)
target_include_directories(test_route_message PRIVATE include ${SDBUS_CPP_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIR})
target_link_libraries(test_route_message PRIVATE GTest::gtest GTest::gtest_main ${SDBUS_CPP_LIBRARIES} ${FMT_LIBRARIES} pthread)
target_compile_options(test_route_message PRIVATE ${SDBUS_CPP_CFLAGS} -Wall -Wextra)
target_link_options(test_route_message PRIVATE ${SDBUS_CPP_LDFLAGS})
add_test(NAME RouteMessageTests COMMAND test_route_message)

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
