cmake_minimum_required(VERSION 3.20)
project(Common VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPDLOG REQUIRED spdlog)
find_package(tomlplusplus QUIET)

add_library(smpp_common STATIC
    src/logger.cpp
    src/config.cpp
)

target_include_directories(smpp_common
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${SPDLOG_INCLUDE_DIRS}
)

target_link_libraries(smpp_common
    PUBLIC  ${SPDLOG_LIBRARIES}
    PRIVATE systemd
)

if(tomlplusplus_FOUND)
    target_link_libraries(smpp_common PUBLIC tomlplusplus::tomlplusplus)
else()
    find_path(TOMLPP_INCLUDE_DIR toml++/toml.hpp
        HINTS /usr/include /usr/local/include
    )
    target_include_directories(smpp_common PUBLIC ${TOMLPP_INCLUDE_DIR})
endif()

target_compile_definitions(smpp_common PRIVATE ${SPDLOG_CFLAGS_OTHER})

# Tests
enable_testing()
find_package(GTest REQUIRED)

add_executable(test_logger tests/test_logger.cpp)
target_link_libraries(test_logger PRIVATE smpp_common GTest::gtest GTest::gtest_main)
add_test(NAME LoggerTests COMMAND test_logger)

add_executable(test_config tests/test_config.cpp)
target_link_libraries(test_config PRIVATE smpp_common GTest::gtest GTest::gtest_main)
add_test(NAME ConfigTests COMMAND test_config)
