#ifdef USE_ALLOC_COUNTER
#include "alloc_counter.h"
#endif
#include "book.h"
#include "generator.h"
#include "alloc_counter.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstdio>

using Clock = std::chrono::steady_clock;

// Compute the value at a given percentile from an ALREADY-SORTED vector.
// p is in [0,100]. Uses nearest-rank: simple, defensible, what you can explain.
static long long percentile(const std::vector<long long>& sorted, double p) {
    if (sorted.empty()) return 0;
    // nearest-rank index, clamped to valid range
    size_t idx = static_cast<size_t>((p / 100.0) * (sorted.size() - 1) + 0.5);
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return sorted[idx];
}

static void report(const char* name, std::vector<long long>& samples_ns,
                   double total_seconds) {
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

    double throughput = samples_ns.size() / total_seconds;

    std::printf("%-8s : n=%zu  min=%lldns  p50=%lldns  p99=%lldns  "
                "p99.9=%lldns  max=%lldns  throughput=%.0f ops/s\n",
                name, samples_ns.size(), mn, p50, p99, p999, mx, throughput);
}

int main(int argc, char** argv) {
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

   #ifdef USE_ALLOC_COUNTER
        alloc_counter::reset();
    #endif
        auto run_start = Clock::now();  
    for (const Action& a : actions) {
        if (a.kind == Action::Cancel) {
            int id = a.cancel_id;
            auto t0 = Clock::now();
            book.cancel(id);
            auto t1 = Clock::now();
            cancel_ns.push_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        } else {
            Order o = a.order; // copy: add() takes by value and mutates
            auto t0 = Clock::now();
            book.add(o);
            auto t1 = Clock::now();
            add_ns.push_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        }
    }

    auto run_end = Clock::now();
    #ifdef USE_ALLOC_COUNTER
        unsigned long long replay_allocs = alloc_counter::count();
    #endif
    double total_s =
        std::chrono::duration_cast<std::chrono::duration<double>>(run_end - run_start)
            .count();

    std::printf("\n--- results ---\n");
    std::printf("total replay time: %.4f s for %zu actions\n",
                total_s, actions.size());
    report("add",    add_ns,    total_s);
    report("cancel", cancel_ns, total_s);
    #ifdef USE_ALLOC_COUNTER
        std::printf("allocations during replay: %llu  (add ops: %zu)\n",
                replay_allocs, add_ns.size());
    #endif
    return 0;
}