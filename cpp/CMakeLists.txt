cmake_minimum_required(VERSION 3.16)
project(OrderBookEngine
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "Low-latency order book matching engine"
)

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Compiler Flags
# ============================================================================
# Release: Maximum optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# Debug: Sanitizers for catching bugs
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")

# ============================================================================
# Options
# ============================================================================
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)

# ============================================================================
# Include Directories
# ============================================================================
include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# External Dependencies (FetchContent)
# ============================================================================
include(FetchContent)

# Google Test - for unit testing
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Prevent GoogleTest from overriding compiler/linker options
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# Google Benchmark - for performance testing
if(BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# ============================================================================
# Core Library
# ============================================================================
add_library(orderbook_core
    src/price_level.cpp
    src/order_book.cpp
)
target_include_directories(orderbook_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# ============================================================================
# Main Executable (Demo)
# ============================================================================
# Uncomment when main.cpp exists:
# add_executable(orderbook_demo src/main.cpp)
# target_link_libraries(orderbook_demo PRIVATE orderbook_core)

# ============================================================================
# Unit Tests
# ============================================================================
if(BUILD_TESTS)
    # Uncomment when test files exist:
    # add_executable(orderbook_tests
    #     tests/test_order.cpp
    #     tests/test_orderbook.cpp
    #     tests/test_matching.cpp
    # )
    # target_link_libraries(orderbook_tests PRIVATE
    #     orderbook_core
    #     GTest::gtest_main
    # )
    # include(GoogleTest)
    # gtest_discover_tests(orderbook_tests)
endif()

# ============================================================================
# Benchmarks
# ============================================================================
if(BUILD_BENCHMARKS)
    # Uncomment when benchmark files exist:
    # add_executable(latency_benchmark benchmarks/latency_benchmark.cpp)
    # target_link_libraries(latency_benchmark PRIVATE
    #     orderbook_core
    #     benchmark::benchmark
    # )
endif()

# ============================================================================
# Print Configuration Summary
# ============================================================================
message(STATUS "")
message(STATUS "=== OrderBookEngine Configuration ===")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests:    ${BUILD_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "=====================================")
message(STATUS "")
