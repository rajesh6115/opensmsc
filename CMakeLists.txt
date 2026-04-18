cmake_minimum_required(VERSION 3.20)

project(
    SimpleSmppServer
    VERSION 1.0.0
    LANGUAGES CXX
)

# C++ Standard Configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# Build configuration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add smppcxx external library (if needed for future phases)
# add_subdirectory(external/smppcxx)

# Add main server project
add_subdirectory(SMPPServer)

# Testing - will be planned with component-level tests
# Individual modules will have their own test subdirectories
# option(BUILD_TESTS "Build unit tests" OFF)
# if(BUILD_TESTS)
#   enable_testing()
#   add_subdirectory(tests)
# endif()

# Print configuration summary
message(STATUS "")
message(STATUS "  ════════════════════════════════════════════════════════════")
message(STATUS "  SimpleSmppServer ${PROJECT_VERSION}")
message(STATUS "  ────────────────────────────────────────────────────────────")
message(STATUS "  C++ Standard         : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler             : ${CMAKE_CXX_COMPILER}")
message(STATUS "  Build Type           : ${CMAKE_BUILD_TYPE}")
message(STATUS "  ════════════════════════════════════════════════════════════")
message(STATUS "")
