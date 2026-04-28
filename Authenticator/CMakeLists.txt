cmake_minimum_required(VERSION 3.20)
project(Authenticator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUS_CPP REQUIRED sdbus-c++)

find_program(SDBUS_CPP_XML2CPP sdbus-c++-xml2cpp REQUIRED)

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

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
find_package(GTest REQUIRED)

add_executable(test_iauth_dbus tests/test_iauth_dbus.cpp)
add_dependencies(test_iauth_dbus generate_iauth_dbus)

target_include_directories(test_iauth_dbus PRIVATE
    include
    ${SDBUS_CPP_INCLUDE_DIRS}
)
target_link_libraries(test_iauth_dbus PRIVATE
    GTest::gtest GTest::gtest_main
    ${SDBUS_CPP_LIBRARIES}
)
target_compile_options(test_iauth_dbus PRIVATE ${SDBUS_CPP_CFLAGS})
target_link_options(test_iauth_dbus PRIVATE ${SDBUS_CPP_LDFLAGS})

add_test(NAME IAuthDbusTests COMMAND test_iauth_dbus)
