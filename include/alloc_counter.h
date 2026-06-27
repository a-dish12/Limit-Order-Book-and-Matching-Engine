#ifndef ALLOC_COUNTER_H
#define ALLOC_COUNTER_H

#include <cstddef>

// Counts heap allocations by replacing the global operator new / delete.
// Single-threaded use only (your benchmark runs add/cancel serially).
// Compile alloc_counter.cpp into exactly ONE target — the overrides are
// program-wide, so two copies would be a duplicate-symbol link error.
namespace alloc_counter {
    void reset();                  // zero the counters
    unsigned long long count();    // number of operator new calls since reset()
    unsigned long long bytes();    // total bytes requested since reset()
}

#endif // ALLOC_COUNTER_H