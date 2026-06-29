// Differential testing harness.
//
// Drives the SAME randomly-generated action stream through the real Book and the
// naive ReferenceBook, comparing the per-action fill stream after every action.
// The reference is trusted (you hand-verified it); any disagreement is reported
// as a real-engine bug with a fully reproducible seed.
//
// What it catches: any case where the two independent implementations DISAGREE.
// What it cannot catch: a bug both engines share (same wrong assumption). That's
// why the reference was written independently, from the spec, not from Book.

#include "book.h"
#include "reference_book.h"
#include <vector>
#include <random>
#include <cstdio>
#include <cstdint>

// ----- one randomly generated action -----
struct GenAction {
    enum Kind { AddLimit, AddMarket, Cancel } kind;
    Order order;       // valid for AddLimit / AddMarket
    int   cancel_id;   // valid for Cancel
};

// ---------------------------------------------------------------------------
// Adversarial generator.
//
// Deliberately concentrates on the corners most likely to expose a priority or
// bookkeeping divergence:
//   - a NARROW price band, so many orders share a price -> stresses price-time
//     priority (the #1 place two engines differ).
//   - cancels that target ALREADY-ISSUED ids, including already-filled ones
//     (cancel-miss path).
//   - market orders (price 0) including into a thin/empty book.
//   - aggressive crossing limits that sweep multiple levels.
// Every order gets a fresh unique id (next_id++), so cancel/fill references are
// never ambiguous and every disagreement is a REAL disagreement.
// ---------------------------------------------------------------------------
static std::vector<GenAction> generate_adversarial(uint64_t seed, int num_actions,
                                                   int mid, int band) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> roll(0, 99);
    std::uniform_int_distribution<int> qty_dist(1, 20);
    std::uniform_int_distribution<int> side_dist(0, 1);
    // NARROW band around mid: forces frequent price ties.
    std::uniform_int_distribution<int> price_dist(mid - band, mid + band);

    std::vector<GenAction> actions;
    actions.reserve(num_actions);
    std::vector<int> issued_ids;          // every id we've ever handed out
    issued_ids.reserve(num_actions);

    int next_id = 1;

    for (int i = 0; i < num_actions; ++i) {
        int r = roll(rng);
        bool buy = side_dist(rng) == 1;
        int  qty = qty_dist(rng);

        if (r < 45) {
            // resting-ish limit somewhere in the band
            int price = price_dist(rng);
            int id = next_id++;
            actions.push_back({GenAction::AddLimit,
                               Order::limit(id, buy, price, qty), 0});
            issued_ids.push_back(id);
        } else if (r < 65) {
            // aggressive crossing limit: buy priced high, sell priced low,
            // so it sweeps. Pushed to the band edge to cross multiple levels.
            int price = buy ? mid + band : mid - band;
            int id = next_id++;
            actions.push_back({GenAction::AddLimit,
                               Order::limit(id, buy, price, qty), 0});
            issued_ids.push_back(id);
        } else if (r < 80) {
            // market order (price 0, ignores price). May hit empty book.
            int id = next_id++;
            actions.push_back({GenAction::AddMarket,
                               Order::market(id, buy, qty), 0});
            issued_ids.push_back(id);
        } else {
            // cancel a previously-ISSUED id (may already be filled -> miss).
            if (issued_ids.empty()) {
                int price = price_dist(rng);
                int id = next_id++;
                actions.push_back({GenAction::AddLimit,
                                   Order::limit(id, buy, price, qty), 0});
                issued_ids.push_back(id);
            } else {
                std::uniform_int_distribution<size_t> pick(0, issued_ids.size() - 1);
                int id = issued_ids[pick(rng)];
                actions.push_back({GenAction::Cancel, Order::limit(0,false,0,0), id});
            }
        }
    }
    return actions;
}

// ---------------------------------------------------------------------------
// Pretty-print a fill stream for failure dumps.
// ---------------------------------------------------------------------------
static void dump_fills(const char* label, const std::vector<Fill>& fills) {
    std::printf("    %s (%zu fills):\n", label, fills.size());
    for (const Fill& f : fills) {
        std::printf("      {taker=%d maker=%d price=%d qty=%d}\n",
                    f.taker_id, f.maker_id, f.price, f.qty);
    }
}

static void dump_action(const GenAction& a) {
    switch (a.kind) {
        case GenAction::AddLimit:
            std::printf("    action: ADD LIMIT id=%d %s price=%d qty=%d\n",
                        a.order.id, a.order.buyside ? "BUY" : "SELL",
                        a.order.price, a.order.qty_remaining);
            break;
        case GenAction::AddMarket:
            std::printf("    action: ADD MARKET id=%d %s qty=%d\n",
                        a.order.id, a.order.buyside ? "BUY" : "SELL", a.order.qty_remaining);
            break;
        case GenAction::Cancel:
            std::printf("    action: CANCEL id=%d\n", a.cancel_id);
            break;
    }
}

// ---------------------------------------------------------------------------
// Run ONE seeded sequence through both engines. Returns true if they agreed on
// every action; on first divergence prints a full reproducible report and
// returns false.
// ---------------------------------------------------------------------------
static bool run_one(uint64_t seed, int num_actions, int mid, int band) {
    std::vector<GenAction> actions =
        generate_adversarial(seed, num_actions, mid, band);

    Book          real(num_actions);   // pool sized to worst case
    ReferenceBook ref;

    for (size_t i = 0; i < actions.size(); ++i) {
        const GenAction& a = actions[i];

        std::vector<Fill> real_fills, ref_fills;
        bool real_cancel = false, ref_cancel = false;
        bool is_cancel = (a.kind == GenAction::Cancel);

        if (is_cancel) {
            real_cancel = real.cancel(a.cancel_id);
            ref_cancel  = ref.cancel(a.cancel_id);
        } else {
            real_fills = real.add(a.order);
            ref_fills  = ref.add(a.order);
        }

        bool mismatch =
            is_cancel ? (real_cancel != ref_cancel)
                      : (real_fills  != ref_fills);

        if (mismatch) {
            std::printf("\n*** DIVERGENCE ***\n");
            std::printf("  seed=%llu  action_index=%zu  (of %d)\n",
                        (unsigned long long)seed, i, num_actions);
            dump_action(a);
            if (is_cancel) {
                std::printf("    real.cancel -> %s   ref.cancel -> %s\n",
                            real_cancel ? "true" : "false",
                            ref_cancel  ? "true" : "false");
            } else {
                dump_fills("REAL", real_fills);
                dump_fills("REF ", ref_fills);
            }
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// main: run many seeded sequences. Stops at the first failing seed so you have
// a single minimal repro to debug. Args: [num_sequences] [actions_per_seq]
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    int num_sequences   = (argc > 1) ? std::atoi(argv[1]) : 100000;
    int actions_per_seq = (argc > 2) ? std::atoi(argv[2]) : 200;

    const int mid  = 100;
    const int band = 3;   // narrow -> lots of price ties on purpose

    std::printf("Differential test: %d sequences x %d actions  "
                "(price band %d..%d)\n",
                num_sequences, actions_per_seq, mid - band, mid + band);

    for (int s = 0; s < num_sequences; ++s) {
        uint64_t seed = 0x9E3779B97F4A7C15ULL ^ (uint64_t)s;  // spread seeds
        if (!run_one(seed, actions_per_seq, mid, band)) {
            std::printf("\nFAILED on sequence %d (seed=%llu). "
                        "Re-run that seed alone to debug.\n",
                        s, (unsigned long long)seed);
            return 1;
        }
        if ((s + 1) % 10000 == 0) {
            std::printf("  %d sequences passed...\n", s + 1);
        }
    }

    std::printf("\nALL %d SEQUENCES AGREE. No divergence found.\n", num_sequences);
    return 0;
}