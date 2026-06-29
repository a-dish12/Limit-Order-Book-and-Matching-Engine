# Minimum CMake version that understands the syntax we use. Safe modern floor.
cmake_minimum_required(VERSION 3.16)

# Project name and language. "CXX" means C++. This sets up default variables.
project(lob LANGUAGES CXX)

# Use C++17 (you rely on structured bindings, std::next, etc.), and require it
# (don't silently fall back to an older standard if the compiler is old).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Turn on warnings. -Wall -Wextra catch the &-vs-&& class of bug at compile time.
add_compile_options(-Wall -Wextra)

# Tell the compiler where to find headers when it sees #include "book.h".
include_directories(${CMAKE_SOURCE_DIR}/include)

# Default build type for lob / lob_tests (benchmarks pin their own -O2 below).
# CACHE ... FORCE makes the default actually stick; the message() prints the
# build type on every configure so a stale Debug cache can never hide silently.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# --- The Book library ---
# Compile src/book.cpp once into a reusable library. Everything that needs Book
# (main, benchmarks, tests) links against this instead of recompiling it.
add_library(book_lib src/book.cpp src/reference_book.cpp)
# Pin the ENGINE to -O2 unconditionally. This is the code the benchmark actually
# times (Book::add / Book::cancel run here, not in the harness), so it must be
# optimized regardless of the global build type — otherwise a Debug cache would
# silently measure unoptimized engine code.
target_compile_options(book_lib PRIVATE -O2)

# --- The main executable ---
add_executable(lob src/main.cpp)
target_link_libraries(lob PRIVATE book_lib)

# --- The test executable ---
add_executable(lob_tests tests/test_main.cpp)
target_link_libraries(lob_tests PRIVATE book_lib)

# --- Benchmark executables ---
# Instrumented build: counts allocations. Uses the naive counting allocator,
# so its LATENCY numbers are perturbed — read it only for the allocation count.
add_executable(lob_bench bench/bench_main.cpp bench/generator.cpp bench/alloc_counter.cpp)
target_link_libraries(lob_bench PRIVATE book_lib)
target_compile_definitions(lob_bench PRIVATE USE_ALLOC_COUNTER)

# Clean build: real system allocator, no counter. These are the TRUE latency
# numbers to compare against the locked Stage 2 baseline.
add_executable(lob_bench_clean bench/bench_main.cpp bench/generator.cpp)
target_link_libraries(lob_bench_clean PRIVATE book_lib)

# Pin both benchmark harnesses to -O2 too, so a measurement can never be
# silently unoptimized no matter what CMAKE_BUILD_TYPE is cached as.
target_compile_options(lob_bench       PRIVATE -O2)
target_compile_options(lob_bench_clean PRIVATE -O2)


add_executable(lob_difftest tests/difftest_main.cpp)
target_link_libraries(lob_difftest PRIVATE book_lib)