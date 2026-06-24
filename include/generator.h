#pragma once

#include "book.h"
#include <vector>
#include <cstdint>

// A single benchmark action. The benchmark replays a vector of these.
// kind tells the runner which Book method to call and time.
struct Action {
    enum Kind { AddLimit, AddMarket, Cancel } kind;
    Order order;   // valid for AddLimit / AddMarket
    int   cancel_id; // valid for Cancel
};

// Tunable knobs for the synthetic stream. Defaults describe a "normal market":
// mostly limits clustered near the touch, a minority crossing to cause matches,
// a steady trickle of market orders and cancels.
struct GenConfig {
    uint64_t seed         = 42;      // fixed seed => identical stream every run
    int      num_actions  = 1000000; // total actions to generate

    int      mid_price    = 10000;   // center of the price distribution (ticks)
    int      price_spread = 50;      // how far around mid prices can land (ticks)

    int      min_qty      = 1;
    int      max_qty      = 100;

    // Mix, as integer weights (need not sum to 100; they're relative).
    int      w_limit_rest  = 70; // limit that likely rests near/at touch
    int      w_limit_cross = 15; // limit priced to cross => causes matching
    int      w_market      = 5;  // market order => walks the book
    int      w_cancel      = 10; // cancel a previously-added resting order
};

// Generates a deterministic action stream from cfg.
// All ids are unique and assigned in order. Cancel actions only ever
// reference ids that were added earlier in the stream (so they exercise
// the O(1) map path; some may still miss if the order was already filled).
std::vector<Action> generate_actions(const GenConfig& cfg);