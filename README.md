# Limit Order Book & Matching Engine (C++)

A limit order book and matching engine written in C++, built to interview-grade
depth: correctness verified by **differential testing against an independent
reference implementation**, performance measured with a **coordinated-omission-aware
open-loop benchmark**, and optimization driven by before/after latency deltas.

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

> **Complexity note (honest):** best bid/ask is O(1) (the front of each side),
> but inserting a resting order locates its price level with a linear
> `std::find_if` scan — **O(number of price levels)**, not O(log n). Replacing
> this with direct tick-indexed lookup is the primary Stage 4 target.

### The pool allocator

Orders no longer live in a `std::list<Order>` (one heap allocation per resting
order). Instead, `OrderPool` reserves one contiguous block of `Order` slots up
front and hands them out via a free list — a singly-linked chain threaded
through the unused slots themselves (a free slot reuses its `next` field to
point at the next free slot). `acquire()` pops the free-list head; `release()`
pushes a slot back. This turns ~one `operator new` per resting order into a
single allocation at construction, and keeps order storage contiguous for better
cache behaviour during matching traversal.

**Invariant:** under this scheme `release()` overwrites a slot's `next` field
with free-list linkage, so the matching loop must read `frontOrder->next`
*before* releasing the node — the same discipline as never holding a stale
iterator across a structural change.

## Build

Requires CMake (≥ 3.16) and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The engine (`book_lib`) and benchmark targets are pinned to `-O2` regardless of
build type, so a benchmark can never be silently measured unoptimized. (This
guards against a real bug encountered during development: a stale `CMakeCache`
holding `CMAKE_BUILD_TYPE=Debug` once caused unoptimized code to be benchmarked,
producing a false 5x regression. The build type is now also printed at configure
time, and the actual `-O` flag can be confirmed in the generated `flags.make`.)

This produces these executables in `build/`:

- `lob` — demo / scratch harness
- `lob_tests` — correctness test suite (hand-traced scenarios + reference sanity)
- `lob_difftest` — **differential test**: drives random sequences through the real
  engine and an independent reference, asserting identical fills
- `lob_bench` — performance benchmark, instrumented with an allocation counter
- `lob_bench_clean` — performance benchmark with the real system allocator (true latency numbers)

## Correctness

Correctness is established in two layers.

**1. Hand-traced edge-case suite.** Each scenario's expected fills and resting
book state are predicted by hand *before* running, then asserted against the
engine's actual output. Scenarios cover an empty book, multi-level sweeps,
over-fills (dry-book remainder), and partial aggressors, plus the four cancel
cases (head, tail, sole order, middle).

```bash
./build/lob_tests
```

**2. Differential testing against an independent reference.** A deliberately
naive second engine (`ReferenceBook` — a flat `std::vector` with linear scans,
correct by inspection, no pool or intrusive list) is implemented independently
from the spec, *not* derived from the fast engine. An adversarial generator
produces random order/cancel streams concentrated on the corners most likely to
expose bugs — a narrow price band (forcing frequent price ties that stress
price-time priority), cancels of already-filled ids, market orders into thin
books, and multi-level crossing sweeps. Both engines replay each seeded sequence;
the per-action fill streams are compared with `==`. On any divergence the harness
dumps the seed, action index, and both fill streams for deterministic replay.

```bash
./build/lob_difftest [num_sequences] [actions_per_seq]   # default 100000 x 200
```

Current status: **all 100,000 sequences agree (20M adversarial actions), no
divergence.**

> **A real bug this caught.** On its first run, differential testing exposed a
> matching bug the hand-traced suite missed: an incoming buy limit priced at 97
> was filling against a resting sell at 102 — *crossing the spread*. Root cause:
> the price-cross check guarded the outer match loop, but the inner loop drained
> multiple price levels per call without re-checking price, so it swept past
> levels that no longer crossed. The hand-traced tests missed it because none had
> an aggressor priced *between* two resting levels. Fixed by moving the cross
> check inside the matching loop, gated on order type so market orders still
> sweep all levels. This is exactly the class of bug differential testing exists
> to find — and a case unit tests are structurally prone to miss.
>
> *Limitation:* differential testing only catches bugs where the two
> implementations disagree; a wrong assumption shared by both would pass. It
> proves consistency between two independent implementations, not absolute
> correctness — which is why the reference is written from the spec, not the code.

## Performance

`lob_bench` generates a deterministic, seeded stream of orders (configurable mix
of resting limits, crossing limits, market orders, and cancels) and reports
per-operation latency percentiles. Determinism (fixed RNG seed) ensures
before/after comparisons measure code changes, not input variation.

`lob_bench` also carries an allocation counter — it replaces the global
`operator new` to count every heap allocation during the replay, which is how the
per-order allocation bottleneck was attributed. Because that override swaps in a
naive allocator, latency from `lob_bench` is perturbed; the true latency numbers
come from `lob_bench_clean`.

```bash
./build/lob_bench_clean [num_actions] [seed]
```

### Measurement methodology

The benchmark is **open-loop and coordinated-omission-aware**. Each operation has
an *intended* start time on a fixed schedule; latency is measured from the
intended start, **not** from when the operation actually ran. This matters because
a naive closed-loop harness (time op, start next op) hides tail latency: when one
operation stalls, the operations queued behind it experience real delay that a
closed-loop harness never records. Measuring against the intended schedule
surfaces that queueing — the single most common way order-book benchmarks
overstate their tail performance.

**Known measurement limits (stated honestly):**

- **Clock resolution.** On Apple Silicon, `std::chrono::steady_clock` is too
  coarse to resolve a single ~40–75 ns operation (paired reads report 0 ns).
  `mach_absolute_time` is finer but still too coarse to trust a sub-100 ns
  *median*. The **tail** percentiles (many ticks wide) are trustworthy; the
  median is reported as resolution-limited.
- **Environment noise.** Absolute tail figures on an unpinned laptop core carry
  OS scheduling jitter. Fully defensible absolute numbers require a pinned,
  isolated core (Linux `isolcpus`/`taskset`) — a known next step, not yet done.

### Stage 3 before / after (relative deltas — locked baseline, same harness & seed)

These figures compare the pre- and post-optimization engine under the **same**
(original, closed-loop) benchmark, so the **deltas are valid** — both sides share
any methodological bias. Absolute tail figures are being re-measured under the
open-loop methodology above; treat the absolute nanosecond values as the
closed-loop baseline, not the final defensible tail.

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
natural next target). The true `max` was largely unchanged — the optimization
tightened the frequent tail (p99.9), not the rare worst case (likely map rehash
or OS hiccup), and this is not claimed otherwise.

## Roadmap

- [x] **Stage 1 — Correctness:** matching, cancel, market orders; hand-traced test suite.
- [x] **Stage 2 — Measurement:** seeded benchmark, p50/p99/p99.9 latency + throughput.
- [x] **Stage 3 — Optimization:** profiled the `add`/`cancel` tail to per-order
      `std::list` heap allocation; replaced it with a pool allocator + intrusive
      doubly-linked list. Tail latency down ~22% (add p99.9) / ~33% (cancel),
      throughput up ~20%, verified against the locked baseline.
- [x] **Tier 0 — Measurement rigor:** rewrote the benchmark open-loop to eliminate
      coordinated omission; characterized timer overhead and clock resolution.
      Methodology complete; final defensible absolute tail awaits an isolated
      Linux core.
- [x] **Tier 1 — Correctness rigor:** differential testing against an independent
      reference over millions of adversarial sequences; caught and fixed a
      spread-crossing matching bug.
- [ ] **Stage 4 (candidate):** replace the `std::list<PriceLevel>` + linear
      `find_if` with direct tick-indexed price-level lookup (O(1) level access);
      pool or replace the `unordered_map` to remove the remaining per-insert
      allocation.

## Project layout

```
include/    book.h, generator.h, alloc_counter.h, reference_book.h
src/        book.cpp, main.cpp, reference_book.cpp
tests/      test_main.cpp, difftest_main.cpp
bench/      bench_main.cpp, generator.cpp, alloc_counter.cpp
CMakeLists.txt
```