cmake_minimum_required(VERSION 3.20)
project(Authenticator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUS_CPP REQUIRED sdbus-c++)
find_program(SDBUS_CPP_XML2CPP sdbus-c++-xml2cpp REQUIRED)

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

# toml++
find_path(TOMLPP_INCLUDE_DIR toml++/toml.hpp
    HINTS /usr/include /usr/local/include)
if(NOT TOMLPP_INCLUDE_DIR)
    message(FATAL_ERROR "toml++ not found. apt install libtomlplusplus-dev")
endif()

# ── Generate IAuth adaptor + proxy ───────────────────────────────────────────
set(IAUTH_XML     "${CMAKE_CURRENT_SOURCE_DIR}/files/dbus/IAuth.xml")
set(IAUTH_ADAPTOR "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/IAuth_adaptor.hpp")
set(IAUTH_PROXY   "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/IAuth_proxy.hpp")

add_custom_command(
    OUTPUT  "${IAUTH_ADAPTOR}" "${IAUTH_PROXY}"
    COMMAND "${SDBUS_CPP_XML2CPP}" "${IAUTH_XML}"
            --adaptor="${IAUTH_ADAPTOR}"
            --proxy="${IAUTH_PROXY}"
    DEPENDS "${IAUTH_XML}"
    COMMENT "Generating IAuth adaptor/proxy from IAuth.xml"
    VERBATIM
)

add_custom_target(generate_iauth_dbus ALL
    DEPENDS "${IAUTH_ADAPTOR}" "${IAUTH_PROXY}"
)

install(FILES "${IAUTH_XML}"
        DESTINATION /usr/share/dbus-1/interfaces/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

# ── Authenticator executable ─────────────────────────────────────────────────

set(AUTH_SOURCES
    src/main.cpp
    src/auth_service.cpp
    src/credentials_store.cpp
)

add_executable(smpp_authenticator ${AUTH_SOURCES})
add_dependencies(smpp_authenticator generate_iauth_dbus)

target_include_directories(smpp_authenticator PRIVATE
    include
    ${SDBUS_CPP_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIR}
    ${TOMLPP_INCLUDE_DIR}
)
target_link_libraries(smpp_authenticator PRIVATE
    ${SDBUS_CPP_LIBRARIES}
    ${FMT_LIBRARIES}
    pthread
)
target_compile_options(smpp_authenticator PRIVATE ${SDBUS_CPP_CFLAGS} -Wall -Wextra)
target_link_options(smpp_authenticator PRIVATE ${SDBUS_CPP_LDFLAGS})

install(TARGETS smpp_authenticator DESTINATION bin)
install(FILES conf/credentials.toml DESTINATION /etc/smpp-server)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
find_package(GTest REQUIRED)

add_executable(test_iauth_dbus tests/test_iauth_dbus.cpp)
add_dependencies(test_iauth_dbus generate_iauth_dbus)
target_include_directories(test_iauth_dbus PRIVATE include ${SDBUS_CPP_INCLUDE_DIRS})
target_link_libraries(test_iauth_dbus PRIVATE GTest::gtest GTest::gtest_main ${SDBUS_CPP_LIBRARIES})
target_compile_options(test_iauth_dbus PRIVATE ${SDBUS_CPP_CFLAGS})
target_link_options(test_iauth_dbus PRIVATE ${SDBUS_CPP_LDFLAGS})
add_test(NAME IAuthDbusTests COMMAND test_iauth_dbus)

add_executable(test_auth_service
    tests/test_auth_service.cpp
    src/auth_service.cpp
    src/credentials_store.cpp
)
add_dependencies(test_auth_service generate_iauth_dbus)
target_include_directories(test_auth_service PRIVATE
    include ${SDBUS_CPP_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIR} ${TOMLPP_INCLUDE_DIR})
target_link_libraries(test_auth_service PRIVATE
    GTest::gtest GTest::gtest_main ${SDBUS_CPP_LIBRARIES} ${FMT_LIBRARIES} pthread)
target_compile_options(test_auth_service PRIVATE ${SDBUS_CPP_CFLAGS} -Wall -Wextra)
target_link_options(test_auth_service PRIVATE ${SDBUS_CPP_LDFLAGS})
add_test(NAME AuthServiceTests COMMAND test_auth_service)
