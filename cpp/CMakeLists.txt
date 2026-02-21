cmake_minimum_required(VERSION 3.16)
project(OrderBookEngine
    VERSION 1.0.0
    LANGUAGES C CXX
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

# On macOS the C++ standard library headers live inside the SDK.
# When cmake sets CMAKE_OSX_SYSROOT the compiler needs an explicit -I to find them.
if(APPLE)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE MACOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    include_directories(SYSTEM ${MACOS_SDK_PATH}/usr/include/c++/v1)
endif()

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
    # Force std::regex backend — the try_compile detection fails on some
    # macOS/Clang setups because it doesn't inherit our CXX_STANDARD flags.
    set(HAVE_STD_REGEX ON CACHE BOOL "" FORCE)
    # Suppress -Wgnu-include-next which Clang-on-macOS promotes to an error
    # inside Google Benchmark's own build when it hits system libc++ headers.
    if(APPLE)
        add_compile_options(-Wno-gnu-include-next)
    endif()
    FetchContent_MakeAvailable(benchmark)
endif()

# ============================================================================
# Core Library
# ============================================================================
add_library(orderbook_core
    src/price_level.cpp
    src/order_book.cpp
    src/redis_publisher.cpp
)
# -fPIC required on Linux when orderbook_core is linked into the pybind11
# shared module (.so). Harmless on macOS.
set_target_properties(orderbook_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(orderbook_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# hiredis - built from source via FetchContent (same pattern as GoogleTest
# and Google Benchmark — no system package required, works on all platforms)
FetchContent_Declare(
    hiredis
    GIT_REPOSITORY https://github.com/redis/hiredis.git
    GIT_TAG v1.2.0
)
set(ENABLE_SSL       OFF CACHE BOOL "" FORCE)
set(DISABLE_TESTS    ON  CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(hiredis)

target_link_libraries(orderbook_core PUBLIC hiredis)
target_include_directories(orderbook_core PUBLIC ${hiredis_SOURCE_DIR})

# ============================================================================
# Main Executable (Demo)
# ============================================================================
add_executable(orderbook_demo src/main.cpp)
target_link_libraries(orderbook_demo PRIVATE orderbook_core)

# ============================================================================
# Unit Tests
# ============================================================================
if(BUILD_TESTS)
    add_executable(orderbook_tests
        tests/test_order.cpp
        tests/test_price_level.cpp
        tests/test_order_book.cpp
    )
    target_link_libraries(orderbook_tests PRIVATE
        orderbook_core
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(orderbook_tests)
endif()

# ============================================================================
# Benchmarks
# ============================================================================
if(BUILD_BENCHMARKS)
    add_executable(latency_benchmark benchmarks/latency_benchmark.cpp)
    target_link_libraries(latency_benchmark PRIVATE
        orderbook_core
        benchmark::benchmark_main
    )
endif()

# ============================================================================
# Python Bindings (pybind11)
# ============================================================================
option(BUILD_BINDINGS "Build Python bindings" ON)

if(BUILD_BINDINGS)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    # Tell pybind11 to reuse the Python3 we already found above,
    # instead of running its own detection (which can pick up the wrong venv)
    set(PYBIND11_FINDPYTHON ON)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.13.6
    )
    FetchContent_MakeAvailable(pybind11)

    pybind11_add_module(orderbook_engine bindings/orderbook_bindings.cpp)
    target_link_libraries(orderbook_engine PRIVATE orderbook_core)
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
