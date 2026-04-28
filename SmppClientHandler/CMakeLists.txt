cmake_minimum_required(VERSION 3.16)
project(smpp_client_handler
    VERSION 0.1.0
    DESCRIPTION "SMPP Client Handler - Single socket client for SMPP protocol"
    HOMEPAGE_URL "https://github.com/rajesh6115/opensmsc"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find packages using pkg-config ────────────────────────────────────────────

find_package(PkgConfig REQUIRED)

# sdbus-c++ (modern C++ D-Bus binding)
# Install: apt install libsdbus-c++-dev (Debian/Ubuntu)
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

# ── Sources ──────────────────────────────────────────────────────────────────

# Find all smppcxx source files
file(GLOB SMPPCXX_SOURCES
    ../external/smppcxx/src/*.cpp
)

# ── Generate D-Bus Adaptors/Proxies from XML ────────────────────────────────

# IClientHandler — owned by this module (adaptor + proxy)
set(ICLIENTHANDLER_XML     "${CMAKE_CURRENT_SOURCE_DIR}/files/dbus/IClientHandler.xml")
set(ICLIENTHANDLER_ADAPTOR "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/IClientHandler_adaptor.hpp")
set(ICLIENTHANDLER_PROXY   "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/IClientHandler_proxy.hpp")

add_custom_command(
    OUTPUT  "${ICLIENTHANDLER_ADAPTOR}" "${ICLIENTHANDLER_PROXY}"
    COMMAND "${SDBUS_CPP_XML2CPP}" "${ICLIENTHANDLER_XML}"
            --adaptor="${ICLIENTHANDLER_ADAPTOR}"
            --proxy="${ICLIENTHANDLER_PROXY}"
    DEPENDS "${ICLIENTHANDLER_XML}"
    COMMENT "Generating IClientHandler adaptor/proxy"
    VERBATIM
)

add_custom_target(generate_iclienthandler_dbus ALL
    DEPENDS "${ICLIENTHANDLER_ADAPTOR}" "${ICLIENTHANDLER_PROXY}"
)

install(FILES "${ICLIENTHANDLER_XML}"
        DESTINATION /usr/share/dbus-1/interfaces/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

# ── Source files ─────────────────────────────────────────────────────────────

# Source files for smpp_client_handler executable
set(SOURCES
    src/main.cpp
    src/smpp_client_handler.cpp
    src/smpp_state_machine_v2.cpp
    src/smpp_states.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Ensure adaptors are generated before building the executable
add_dependencies(${PROJECT_NAME} generate_iclienthandler_dbus)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    "${GENERATED_DBUS_DIR}"
    ${SPDLOG_INCLUDE_DIR}
    ../external/smppcxx/include
    ${SDBUS_CPP_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    asio_iface
    ${SDBUS_CPP_LIBRARIES}
    ${FMT_LIBRARIES}
    pthread
)

# Add compile flags
target_compile_options(${PROJECT_NAME} PRIVATE
    ${SDBUS_CPP_CFLAGS}
    -Wall -Wextra -Wpedantic
)

# Add link flags
target_link_options(${PROJECT_NAME} PRIVATE
    ${SDBUS_CPP_LDFLAGS}
)

# ── Install ──────────────────────────────────────────────────────────────────

# Install executable
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Install D-Bus interface definition
# This allows other services/modules to generate proxy/client code from the interface
install(FILES files/dbus/com.smmc.SmppClient.xml
        DESTINATION /usr/share/dbus-1/interfaces/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
find_package(GTest REQUIRED)

add_executable(test_iclienthandler_dbus tests/test_iclienthandler_dbus.cpp)
add_dependencies(test_iclienthandler_dbus generate_iclienthandler_dbus)
target_include_directories(test_iclienthandler_dbus PRIVATE include ${SDBUS_CPP_INCLUDE_DIRS})
target_link_libraries(test_iclienthandler_dbus PRIVATE GTest::gtest GTest::gtest_main ${SDBUS_CPP_LIBRARIES})
target_compile_options(test_iclienthandler_dbus PRIVATE ${SDBUS_CPP_CFLAGS})
target_link_options(test_iclienthandler_dbus PRIVATE ${SDBUS_CPP_LDFLAGS})
add_test(NAME IClientHandlerDbusTests COMMAND test_iclienthandler_dbus)

message(STATUS "")
message(STATUS "  ═══════════════════════════════════════════════════════")
message(STATUS "  smpp_client_handler ${PROJECT_VERSION}")
message(STATUS "  ───────────────────────────────────────────────────────")
message(STATUS "  ASIO (standalone) : ${ASIO_INCLUDE_DIR}")
message(STATUS "  sdbus-c++ (D-Bus) : ${SDBUS_CPP_VERSION}")
message(STATUS "  sdbus-c++ xml2cpp : ${SDBUS_CPP_XML2CPP}")
message(STATUS "  Generated DBus    : ${GENERATED_DBUS_DIR}")
message(STATUS "  D-Bus Interface   : /usr/share/dbus-1/interfaces/com.smmc.SmppClient.xml")
message(STATUS "  spdlog            : ${SPDLOG_INCLUDE_DIR} (header-only)")
message(STATUS "  C++ Standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler          : ${CMAKE_CXX_COMPILER}")
message(STATUS "  ═══════════════════════════════════════════════════════")
message(STATUS "")
