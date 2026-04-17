cmake_minimum_required(VERSION 3.16)
project(simple_smpp_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Standalone ASIO (header-only, no Boost) ─────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

# ASIO exposes its headers under <repo>/asio/include
add_library(asio_iface INTERFACE)
target_include_directories(asio_iface INTERFACE
    $<BUILD_INTERFACE:${asio_SOURCE_DIR}/asio/include>
)
target_compile_definitions(asio_iface INTERFACE
    ASIO_STANDALONE          # No Boost dependency
    ASIO_NO_DEPRECATED
)

# ── sdbus-c++ (modern C++ binding over sd-bus / D-Bus) ──────────────────────
# Install: apt install libsdbus-c++-dev   (Debian/Ubuntu)
#          dnf install sdbus-cpp-devel     (Fedora/RHEL)
# Or build from source: https://github.com/Kistler-Group/sdbus-cpp
find_package(sdbus-c++ REQUIRED)

# ── Sources ──────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/tcp_server.cpp
    src/ip_validator.cpp
    src/smpp_service_manager.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    asio_iface
    sdbus-c++::sdbus-c++
    pthread
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)

# ── Install ──────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES config/allowed_ips.conf
        DESTINATION /etc/simple_smpp_server)
install(FILES systemd/SMPPAuthenticator@.service
        DESTINATION /etc/systemd/system)

message(STATUS "")
message(STATUS "  simple_smpp_server ${PROJECT_VERSION}")
message(STATUS "  ASIO             : ${asio_SOURCE_DIR}/asio/include")
message(STATUS "  sdbus-c++        : ${sdbus-c++_VERSION}")
message(STATUS "")
