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

# Source files for smpp_client_handler executable
set(SOURCES
    src/main.cpp
    src/smpp_client_handler.cpp
    src/smpp_state_machine_v2.cpp
    src/smpp_states.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
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
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

message(STATUS "")
message(STATUS "  ═══════════════════════════════════════════════════════")
message(STATUS "  smpp_client_handler ${PROJECT_VERSION}")
message(STATUS "  ───────────────────────────────────────────────────────")
message(STATUS "  ASIO (standalone) : ${ASIO_INCLUDE_DIR}")
message(STATUS "  sdbus-c++ (D-Bus) : ${SDBUS_CPP_VERSION}")
message(STATUS "  spdlog            : ${SPDLOG_INCLUDE_DIR} (header-only)")
message(STATUS "  C++ Standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler          : ${CMAKE_CXX_COMPILER}")
message(STATUS "  ═══════════════════════════════════════════════════════")
message(STATUS "")
