cmake_minimum_required(VERSION 3.16)
project(smpp_client_handler
    VERSION 0.1.0
    DESCRIPTION "SMPP Client Handler - Process-per-session SMPP handler"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUS_CPP REQUIRED sdbus-c++)

find_program(SDBUS_CPP_XML2CPP sdbus-c++-xml2cpp
    HINTS /usr/bin /usr/local/bin REQUIRED)

set(GENERATED_DBUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# spdlog
pkg_check_modules(SPDLOG QUIET spdlog)
if(NOT SPDLOG_FOUND)
    find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h
        HINTS /usr/include /usr/local/include)
    if(NOT SPDLOG_INCLUDE_DIR)
        message(FATAL_ERROR "spdlog not found")
    endif()
else()
    set(SPDLOG_INCLUDE_DIR ${SPDLOG_INCLUDEDIR})
endif()

pkg_check_modules(FMT QUIET fmt)
if(NOT FMT_FOUND)
    find_library(FMT_LIBRARY fmt)
endif()

# ASIO standalone
if(NOT TARGET asio_iface)
    find_path(ASIO_INCLUDE_DIR asio.hpp
        HINTS /usr/include /usr/local/include)
    if(NOT ASIO_INCLUDE_DIR)
        message(FATAL_ERROR "ASIO not found. apt install libasio-dev")
    endif()
    add_library(asio_iface INTERFACE)
    target_include_directories(asio_iface INTERFACE ${ASIO_INCLUDE_DIR})
    target_compile_definitions(asio_iface INTERFACE ASIO_STANDALONE)
endif()

# Also need IServer_adaptor.hpp (from SMPPServer) and IAuth_adaptor.hpp (from Authenticator)
# for the test stubs. Locate the installed/generated files.
set(ISERVER_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../SMPPServer/include")
set(IAUTH_GEN_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../Authenticator/include")

# ── Generate IClientHandler adaptor/proxy ────────────────────────────────────
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

# ── Session library (shared between exe + tests) ──────────────────────────────
add_library(smpp_session_lib STATIC src/smpp_session.cpp)
add_dependencies(smpp_session_lib generate_iclienthandler_dbus)
target_include_directories(smpp_session_lib PUBLIC
    include ${SDBUS_CPP_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIR})
target_link_libraries(smpp_session_lib PUBLIC
    asio_iface ${SDBUS_CPP_LIBRARIES} ${FMT_LIBRARIES} pthread)
target_compile_options(smpp_session_lib PUBLIC ${SDBUS_CPP_CFLAGS} -Wall -Wextra)
target_link_options(smpp_session_lib PUBLIC ${SDBUS_CPP_LDFLAGS})

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} src/main.cpp)
add_dependencies(${PROJECT_NAME} generate_iclienthandler_dbus)
target_link_libraries(${PROJECT_NAME} PRIVATE smpp_session_lib)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

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

add_executable(test_smpp_session tests/test_smpp_session.cpp)
add_dependencies(test_smpp_session generate_iclienthandler_dbus)
target_include_directories(test_smpp_session PRIVATE
    include ${SDBUS_CPP_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIR}
    ${ISERVER_GEN_DIR} ${IAUTH_GEN_DIR})
target_link_libraries(test_smpp_session PRIVATE
    smpp_session_lib GTest::gtest GTest::gtest_main)
add_test(NAME SmppSessionTests COMMAND test_smpp_session)

# PDU helpers test (no D-Bus needed)
add_executable(test_smpp_pdu tests/test_smpp_pdu.cpp)
target_include_directories(test_smpp_pdu PRIVATE include)
target_link_libraries(test_smpp_pdu PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME SmppPduTests COMMAND test_smpp_pdu)

message(STATUS "smpp_client_handler ${PROJECT_VERSION} configured")
