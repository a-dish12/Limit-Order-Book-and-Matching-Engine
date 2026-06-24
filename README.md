# Limit Order Book & Matching Engine (C++)

A limit order book and matching engine written in C++, built to interview-grade
depth: correctness verified by hand-traced tests, performance measured with a
deterministic benchmark, and optimization driven by before/after latency deltas.

## Design

- **Integer tick prices** — no floating point in the matching path.
- **Price-time priority** — best price at the front of each side; FIFO within a
  price level (queue position encodes time priority, no timestamps needed).
- **O(1) cancellation** — a hash map from order id to its location
  (`{price-level iterator, order iterator}`) allows cancel without scanning.

### Data structures

| Purpose | Structure |
|---|---|
| Each side of the book | `std::list<PriceLevel>` (buys descending, sells ascending — best at `front()`) |
| Orders within a price level | `std::list<Order>` (FIFO) |
| Order lookup for cancel | `std::unordered_map<int, OrderLocation>` |

`add()` returns a `std::vector<Fill>` describing every match (one `Fill` per
maker the incoming order trades against, at the maker's price), so callers can
reconstruct full vs. partial fills and average execution price.

## Build

Requires CMake (≥ 3.16) and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This produces three executables in `build/`:

- `lob` — demo / scratch harness
- `lob_tests` — correctness test suite
- `lob_bench` — performance benchmark

## Correctness

Matching correctness is verified by a hand-traced edge-case suite: each scenario's
expected fills and resting book state are predicted by hand *before* running, then
asserted against the engine's actual output. Scenarios cover an empty book,
multi-level sweeps, over-fills (dry-book remainder), and partial aggressors, plus
the four cancel cases.

```bash
./build/lob_tests
```

## Performance

`lob_bench` generates a deterministic, seeded stream of orders (configurable mix
of resting limits, crossing limits, market orders, and cancels) and reports
per-operation latency percentiles and throughput. Determinism (fixed RNG seed)
ensures before/after comparisons measure code changes, not input variation.

```bash
./build/lob_bench [num_actions] [seed]
```

**Current baseline** (1M actions, seed 42, `-O2`, Apple Silicon):

| Operation | p50 | p99 | p99.9 | throughput |
|---|---|---|---|---|
| `add` | ~42 ns | ~250 ns | ~375 ns | ~10M ops/s |
| `cancel` | ~41 ns | ~125 ns | ~250 ns | ~1.2M ops/s |

## Roadmap

- [x] **Stage 1 — Correctness:** matching, cancel, market orders; hand-traced test suite.
- [x] **Stage 2 — Measurement:** seeded benchmark, p50/p99/p99.9 latency + throughput.
- [ ] **Stage 3 — Optimization:** profile the latency tail and reduce it, reporting
      before/after deltas against the same benchmark. (The `add` tail is dominated
      by per-node heap allocation in `std::list` — the primary target.)

## Project layout

```
include/    book.h, generator.h
src/        book.cpp, main.cpp
tests/      test_main.cpp
bench/      bench_main.cpp, generator.cpp
CMakeLists.txt
```