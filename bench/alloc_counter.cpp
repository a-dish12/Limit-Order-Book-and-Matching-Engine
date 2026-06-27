#include "alloc_counter.h"

#include <cstdlib>   // std::malloc, std::free
#include <new>       // std::bad_alloc

namespace {
    unsigned long long g_count = 0;
    unsigned long long g_bytes = 0;
}

namespace alloc_counter {
    void reset()               { g_count = 0; g_bytes = 0; }
    unsigned long long count() { return g_count; }
    unsigned long long bytes() { return g_bytes; }
}

// --- global replacement operators ----------------------------------------
// Every `new` in the program routes here, including the per-node allocations
// inside std::list and unordered_map. That is exactly what we want to count.

void* operator new(std::size_t size) {
    g_count += 1;
    g_bytes += size;
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    g_count += 1;
    g_bytes += size;
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept              { std::free(p); }
void operator delete[](void* p) noexcept            { std::free(p); }

// Sized deletes (C++14+, may be selected under -O2). Forward to free.
void operator delete(void* p, std::size_t) noexcept   { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }