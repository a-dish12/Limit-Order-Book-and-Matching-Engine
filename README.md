# Limit Order Book & Matching Engine (C++)

A limit order book and matching engine written in C++, built to interview-grade
depth: correctness verified by hand-traced tests, performance measured with a
deterministic benchmark, and optimization driven by before/after latency deltas.

## Design

- **Integer tick prices** — no floating point in the matching path.
- **Price-time priority** — best price at the front of each side; FIFO within a
  price level (queue position encodes time priority, no timestamps needed).
- **O(1) cancellation** — a hash map from order id to its location
  (`{price-level iterator, order pointer}`) allows cancel without scanning, and
  the intrusive doubly-linked list gives O(1) unlink via the order's own
  neighbour pointers.

### Data structures

| Purpose | Structure |
|---|---|
| Each side of the book | `std::list<PriceLevel>` (buys descending, sells ascending — best at `front()`) |
| Orders within a price level | Intrusive doubly-linked list (`head`/`tail` + per-order `next`/`prev`), backed by a pool allocator |
| Order storage | `OrderPool` — one pre-allocated block of `Order` slots + a free list |
| Order lookup for cancel | `std::unordered_map<int, OrderLocation>` |

`add()` returns a `std::vector<Fill>` describing every match (one `Fill` per
maker the incoming order trades against, at the maker's price), so callers can
reconstruct full vs. partial fills and average execution price.

### The pool allocator

Orders no longer live in a `std::list<Order>` (one heap allocation per resting
order). Instead, `OrderPool` reserves one contiguous block of `Order` slots up
front and hands them out via a free list — a singly-linked chain threaded
through the unused slots themselves (a free slot reuses its `next` field to
point at the next free slot). `acquire()` pops the free-list head; `release()`
pushes a slot back. This turns ~one `operator new` per resting order into a
single allocation at construction, and keeps order storage contiguous for better
cache behaviour during matching traversal.

## Build

Requires CMake (≥ 3.16) and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The engine (`book_lib`) and benchmark targets are pinned to `-O2` regardless of
build type, so a benchmark can never be silently measured unoptimized.

This produces these executables in `build/`:

- `lob` — demo / scratch harness
- `lob_tests` — correctness test suite
- `lob_bench` — performance benchmark, instrumented with an allocation counter
- `lob_bench_clean` — performance benchmark with the real system allocator (true latency numbers)

## Correctness

Matching correctness is verified by a hand-traced edge-case suite: each scenario's
expected fills and resting book state are predicted by hand *before* running, then
asserted against the engine's actual output. Scenarios cover an empty book,
multi-level sweeps, over-fills (dry-book remainder), and partial aggressors, plus
the four cancel cases (head, tail, sole order, middle).

```bash
./build/lob_tests
```

## Performance

`lob_bench` generates a deterministic, seeded stream of orders (configurable mix
of resting limits, crossing limits, market orders, and cancels) and reports
per-operation latency percentiles and throughput. Determinism (fixed RNG seed)
ensures before/after comparisons measure code changes, not input variation.

`lob_bench` also carries an allocation counter — it replaces the global
`operator new` to count every heap allocation during the replay, which is how the
per-order allocation bottleneck was attributed. Because that override swaps in a
naive allocator, latency from `lob_bench` is perturbed; the true latency numbers
come from `lob_bench_clean`.

```bash
./build/lob_bench_clean [num_actions] [seed]
```

### Before / after (1M actions, seed 42, `-O2`, Apple Silicon)

| Operation | metric | before (`std::list`) | after (pool) | change |
|---|---|---|---|---|
| `add` | p50 | ~42 ns | ~42 ns | flat |
| `add` | p99 | ~250 ns | ~209 ns | −16% |
| `add` | p99.9 | ~375 ns | ~292 ns | −22% |
| `add` | throughput | ~10M ops/s | ~12M ops/s | +20% |
| `cancel` | p99 | ~125 ns | ~84 ns | −33% |
| `cancel` | p99.9 | ~250 ns | ~167 ns | −33% |

The median is unchanged — the common-case order never paid a dominant allocation
cost. The win is in the **tail**, exactly where allocation stalls and cache
misses concentrate. Heap allocations per 1M-action run dropped from ~2.1M to
~1.6M (the remainder is the `unordered_map`'s own per-insert allocation, the
natural next target).

## Roadmap

- [x] **Stage 1 — Correctness:** matching, cancel, market orders; hand-traced test suite.
- [x] **Stage 2 — Measurement:** seeded benchmark, p50/p99/p99.9 latency + throughput.
- [x] **Stage 3 — Optimization:** profiled the `add`/`cancel` tail to per-order
      `std::list` heap allocation; replaced it with a pool allocator + intrusive
      doubly-linked list. Tail latency down ~22% (add p99.9) / ~33% (cancel),
      throughput up ~20%, verified against the locked baseline.
- [ ] **Stage 4 (candidate):** pool or replace the `unordered_map` to remove the
      remaining per-insert allocation; price-level lookup by direct tick index.

## Project layout

```
include/    book.h, generator.h, alloc_counter.h
src/        book.cpp, main.cpp
tests/      test_main.cpp
bench/      bench_main.cpp, generator.cpp, alloc_counter.cpp
CMakeLists.txt
```