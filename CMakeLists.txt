# Minimum CMake version that understands the syntax we use. Safe modern floor.
cmake_minimum_required(VERSION 3.16)

# Project name and language. "CXX" means C++. This sets up default variables.
project(lob LANGUAGES CXX)

# Use C++17 (you rely on structured bindings, std::next, etc.), and require it
# (don't silently fall back to an older standard if the compiler is old).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Turn on warnings. -Wall -Wextra catch the &-vs-&& class of bug at compile time.
# This is the line that would have flagged your bitwise-AND typo.
add_compile_options(-Wall -Wextra)

# Tell the compiler where to find headers when it sees #include "book.h".
# Without this, #include "book.h" from src/ or tests/ wouldn't be found.
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- The Book library ---
# Compile src/book.cpp into a reusable library object. Everything that needs
# Book (main, benchmarks, tests) links against this instead of recompiling it.
# THIS is what makes Stage 2 benchmarking clean: one compiled Book, many harnesses.
add_library(book_lib src/book.cpp)

# --- The main executable ---
# Build the program from src/main.cpp, then link it to book_lib so the linker
# can fill in the Book method bodies that main.cpp only declared via the header.
add_executable(lob src/main.cpp)
target_link_libraries(lob PRIVATE book_lib)

# --- The test executable ---
# A separate program built from your test file, also linked to book_lib.
# Lets you run engine tests independently of main.
add_executable(lob_tests tests/test_main.cpp)
target_link_libraries(lob_tests PRIVATE book_lib)