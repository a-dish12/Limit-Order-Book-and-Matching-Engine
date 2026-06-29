#ifdef USE_ALLOC_COUNTER
#include "alloc_counter.h"
#endif
#include "book.h"
#include "generator.h"
#include "alloc_counter.h"
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <mach/mach_time.h>

// High-resolution clock for Apple Silicon.
// std::chrono::steady_clock is too COARSE here: it reported 0ns for back-to-back
// reads and 0ns minimums, meaning it cannot resolve a ~40ns operation. Its tick
// is larger than the thing we're timing.
// mach_absolute_time() returns raw ticks; mach_timebase_info() gives the
// ticks->nanoseconds conversion for THIS machine. Multiply to get real ns.
struct MachClock {
    static double ns_per_tick;
    static void init() {
        mach_timebase_info_data_t tb;
        mach_timebase_info(&tb);
        ns_per_tick = static_cast<double>(tb.numer) / tb.denom;
    }
    static long long now_ns() {
        return static_cast<long long>(mach_absolute_time() * ns_per_tick);
    }
};
double MachClock::ns_per_tick = 1.0;

// Compute the value at a given percentile from an ALREADY-SORTED vector.
// p is in [0,100]. Uses nearest-rank: simple, defensible, what you can explain.
static long long percentile(const std::vector<long long>& sorted, double p) {
    if (sorted.empty()) return 0;
    // nearest-rank index, clamped to valid range
    size_t idx = static_cast<size_t>((p / 100.0) * (sorted.size() - 1) + 0.5);
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return sorted[idx];
}

static void report(const char* name, std::vector<long long>& samples_ns) {
    if (samples_ns.empty()) {
        std::printf("%-8s : no samples\n", name);
        return;
    }
    std::sort(samples_ns.begin(), samples_ns.end());

    long long p50  = percentile(samples_ns, 50.0);
    long long p99  = percentile(samples_ns, 99.0);
    long long p999 = percentile(samples_ns, 99.9);
    long long mn   = samples_ns.front();
    long long mx   = samples_ns.back();

    std::printf("%-8s : n=%zu  min=%lldns  p50=%lldns  p99=%lldns  "
                "p99.9=%lldns  max=%lldns\n",
                name, samples_ns.size(), mn, p50, p99, p999, mx);
}

// Measure the cost of the timing apparatus itself: back-to-back clock reads.
// We subtract nothing automatically — we REPORT it so percentiles are interpretable.
// Returns median ns of a single now_ns() pair-delta.
static long long measure_timer_overhead(int iters = 200000) {
    std::vector<long long> deltas;
    deltas.reserve(iters);
    for (int i = 0; i < iters; ++i) {
        long long a = MachClock::now_ns();
        long long b = MachClock::now_ns();
        deltas.push_back(b - a);
    }
    std::sort(deltas.begin(), deltas.end());
    return deltas[deltas.size() / 2];   // median
}

int main(int argc, char** argv) {
    MachClock::init();   // must run before any now_ns() call

    GenConfig cfg;
    if (argc > 1) cfg.num_actions = std::atoi(argv[1]);
    if (argc > 2) cfg.seed        = static_cast<uint64_t>(std::atoll(argv[2]));

    std::printf("Generating %d actions (seed=%llu)...\n",
                cfg.num_actions, (unsigned long long)cfg.seed);
    std::vector<Action> actions = generate_actions(cfg);

    Book book(cfg.num_actions);

    // Per-operation latency samples, kept separate for the two hot paths.
    std::vector<long long> add_ns, cancel_ns;
    add_ns.reserve(actions.size());
    cancel_ns.reserve(actions.size());

    // --- timer overhead, measured once, reported alongside percentiles ---
    long long timer_overhead_ns = measure_timer_overhead();
    std::printf("timer overhead (median of paired now()): %lldns\n",
                timer_overhead_ns);

    // --- open-loop pacing config ---
    // Drive at a fixed target rate. intended_start[i] = run_start + i*interval.
    // Latency is measured from intended start, NOT actual start: this is the
    // coordinated-omission fix.
    // Rate dropped to 3M (from 9.6M) so the engine has SLACK: at 100% capacity
    // the tail was pure pacing backlog, not engine behaviour. With slack, the
    // tail reflects the engine's own stalls (allocations, rehashes).
    const double target_rate = 3.0e6;                      // ops/sec — YOUR knob
    const long long interval_ns =
        static_cast<long long>(1e9 / target_rate);         // ~333ns at 3M

    #ifdef USE_ALLOC_COUNTER
        alloc_counter::reset();
    #endif

    long long run_start = MachClock::now_ns();
    for (size_t i = 0; i < actions.size(); ++i) {
        const Action& a = actions[i];

        // intended start time for op i under the fixed schedule
        long long intended = run_start + static_cast<long long>(i) * interval_ns;

        // open-loop pacing: if we're ahead of schedule, wait until intended.
        // if we're BEHIND (a previous op stalled), don't wait — we want to
        // record the queueing delay, not hide it.
        while (MachClock::now_ns() < intended) { /* spin */ }

        if (a.kind == Action::Cancel) {
            int id = a.cancel_id;
            book.cancel(id);
            long long done = MachClock::now_ns();
            cancel_ns.push_back(done - intended);
        } else {
            Order o = a.order;
            book.add(o);
            long long done = MachClock::now_ns();
            add_ns.push_back(done - intended);
        }
    }

    long long run_end = MachClock::now_ns();
    #ifdef USE_ALLOC_COUNTER
        unsigned long long replay_allocs = alloc_counter::count();
    #endif
    double total_s = (run_end - run_start) / 1e9;

    std::printf("\n--- results ---\n");
    std::printf("total replay time: %.4f s for %zu actions\n",
                total_s, actions.size());
    report("add",    add_ns);
    report("cancel", cancel_ns);
    #ifdef USE_ALLOC_COUNTER
        std::printf("allocations during replay: %llu  (add ops: %zu)\n",
                replay_allocs, add_ns.size());
    #endif
    return 0;
}