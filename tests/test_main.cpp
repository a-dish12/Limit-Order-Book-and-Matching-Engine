#include "book.h"
#include <iostream>
#include <vector>
#include "reference_book.h"
#include <cassert>
#include <cstdio>



// ---- tiny assertion harness (no external deps) ----
static int g_failures = 0;
static int g_checks   = 0;

#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        ++g_checks;                                                         \
        auto _a = (actual);                                                 \
        auto _e = (expected);                                               \
        if (_a != _e) {                                                     \
            ++g_failures;                                                   \
            std::cout << "  FAIL [" << __func__ << ":" << __LINE__ << "] "  \
                      << #actual << " == " << #expected                     \
                      << "  (got " << _a << ", expected " << _e << ")\n";   \
        }                                                                   \
    } while (0)

// Helper: assert a single Fill matches {taker, maker, price, qty}
static void check_fill(const Fill& f, int taker, int maker, int price, int qty,
                       const char* tag) {
    ++g_checks;
    if (f.taker_id != taker || f.maker_id != maker ||
        f.price != price || f.qty != qty) {
        ++g_failures;
        std::cout << "  FAIL [" << tag << "] fill mismatch: got {"
                  << f.taker_id << "," << f.maker_id << "," << f.price << ","
                  << f.qty << "} expected {" << taker << "," << maker << ","
                  << price << "," << qty << "}\n";
    }
}

// =====================================================================
// Scenario 1: EMPTY BOOK — one limit order rests, no match.
//   Order sequence is wired. FILL IN the expected values from your trace.
// =====================================================================
static void scenario_1_empty_book() {
    std::cout << "Scenario 1: empty book\n";
    Book b(64);
    auto fills = b.add(Order::limit(1, /*buyside=*/true, /*price=*/100, /*qty=*/50));

    CHECK_EQ(fills.size(), 0u);     // empty book: nothing to match
    CHECK_EQ(b.best_bid(), 100);    // the order rests on the buy side
    CHECK_EQ(b.best_ask(), -1);     // sell side is empty
    CHECK_EQ(b.qty_at(true, 100), 50);
}

// =====================================================================
// Scenario 2: TWO-LEVEL SWEEP  (traced + corrected together)
//   sells rest at 100(50) and 101(40); buy 80 @ 101 crosses both.
// =====================================================================
static void scenario_2_two_level_sweep() {
    std::cout << "Scenario 2: two-level sweep\n";
    Book b(64);
    b.add(Order::limit(1, /*sell*/false, 100, 50));
    b.add(Order::limit(2, /*sell*/false, 101, 40));
    auto fills = b.add(Order::limit(3, /*buy*/true, 101, 80));

    CHECK_EQ(fills.size(), 2u);
    if (fills.size() == 2) {
        check_fill(fills[0], /*taker*/3, /*maker*/1, /*price*/100, /*qty*/50, "S2.fill0");
        check_fill(fills[1], /*taker*/3, /*maker*/2, /*price*/101, /*qty*/30, "S2.fill1");
    }
    CHECK_EQ(b.best_bid(), -1);   // buy fully filled, nothing rests on buy side
    CHECK_EQ(b.best_ask(), 101);  // maker 2 leftover
    CHECK_EQ(b.qty_at(false, 101), 10);
    CHECK_EQ(b.qty_at(true, 100), -1); // nothing on buy side
}

// =====================================================================
// Scenario 3: OVER-FILL / DRY BOOK — aggressor wants more than rests.
//   sells rest at 100(50) only; buy 80 @ 105 (or market) — 30 unfilled.
//   FILL IN expected values. Decide limit-vs-market for the aggressor.
// =====================================================================
static void scenario_3_over_fill() {
    std::cout << "Scenario 3: over-fill / dry book\n";
    Book b(64);
    b.add(Order::limit(1, /*sell*/false, 100, 50));
    auto fills = b.add(Order::limit(2, /*buy*/true, 105, 80));
    // (swap to Order::market(2, true, 80) if that's the case you traced)

    CHECK_EQ(fills.size(), 1u);     // only 50 available on the sell side
    if (fills.size() == 1) {
        check_fill(fills[0], /*taker*/2, /*maker*/1, /*price*/100, /*qty*/50, "S3.fill0");
    }
    CHECK_EQ(b.best_ask(), -1);     // sell side fully consumed
    CHECK_EQ(b.best_bid(), 105);    // 30 remainder rests on buy side at the limit price
    CHECK_EQ(b.qty_at(true, 105), 30);
}

// =====================================================================
// Scenario 4: PARTIAL AGGRESSOR — aggressor smaller than the maker it hits.
//   sell rests at 100(50); buy 20 @ 100 — one fill, maker keeps 30.
//   FILL IN expected values.
// =====================================================================
static void scenario_4_partial_aggressor() {
    std::cout << "Scenario 4: partial aggressor\n";
    Book b(64);
    b.add(Order::limit(1, /*sell*/false, 100, 50));
    auto fills = b.add(Order::limit(2, /*buy*/true, 100, 20));

    CHECK_EQ(fills.size(), 1u);
    if (fills.size() == 1) {
        check_fill(fills[0], /*taker*/2, /*maker*/1, /*price*/100, /*qty*/20, "S4.fill0");
    }
    CHECK_EQ(b.best_ask(), 100);        // maker still rests with leftover
    CHECK_EQ(b.qty_at(false, 100), 30); // maker leftover
    CHECK_EQ(b.best_bid(), -1);         // aggressor fully filled, nothing rests on buy
}

// =====================================================================
// Reference engine sanity: market order partial-fills a resting ask.
//   ask id30 sell 10 @ 200; market BUY 4 -> one fill (31,30,200,4), 6 rest.
//   Uses the same counted CHECK_EQ / check_fill harness as the scenarios.
// =====================================================================
static void test_reference_market_partial_fill() {
    std::cout << "Scenario 5: Reference: market partial fill\n";
    ReferenceBook ref;

    auto f1 = ref.add(Order::limit(30, /*buyside=*/false, /*price=*/200, /*qty=*/10));
    CHECK_EQ(f1.size(), 0u);   // rests, no match yet

    auto f2 = ref.add(Order::market(31, /*buyside=*/true, /*qty=*/4));
    CHECK_EQ(f2.size(), 1u);
    if (f2.size() == 1) {
        check_fill(f2[0], /*taker*/31, /*maker*/30, /*price*/200, /*qty*/4, "REF.fill0");
    }
}

int main() {
    scenario_1_empty_book();
    scenario_2_two_level_sweep();
    scenario_3_over_fill();
    scenario_4_partial_aggressor();
    test_reference_market_partial_fill();

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks
              << " checks passed.\n";
    if (g_failures == 0) {
        std::cout << "ALL GREEN\n";
        return 0;
    }
    std::cout << g_failures << " FAILURE(S)\n";
    return 1;
}