#include "generator.h"
#include <random>

std::vector<Action> generate_actions(const GenConfig& cfg) {
    std::mt19937_64 rng(cfg.seed);              // seeded => reproducible
    std::uniform_int_distribution<int> qty_dist(cfg.min_qty, cfg.max_qty);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> mix_dist(
        0, cfg.w_limit_rest + cfg.w_limit_cross + cfg.w_market + cfg.w_cancel - 1);

    // Resting limits land within +/- price_spread of mid; crossing limits are
    // priced aggressively (a buy priced high, a sell priced low) so they cross.
    std::uniform_int_distribution<int> rest_off(-cfg.price_spread, cfg.price_spread);
    std::uniform_int_distribution<int> cross_off(1, cfg.price_spread);

    std::vector<Action> actions;
    actions.reserve(cfg.num_actions);

    // Track ids we've added so cancels reference real, earlier ids.
    std::vector<int> live_ids;
    live_ids.reserve(cfg.num_actions);

    int next_id = 1;

    const int t_rest   = cfg.w_limit_rest;
    const int t_cross  = t_rest + cfg.w_limit_cross;
    const int t_market = t_cross + cfg.w_market;
    // anything >= t_market and < total is a cancel

    for (int i = 0; i < cfg.num_actions; ++i) {
        int roll = mix_dist(rng);
        bool buy = side_dist(rng) == 1;
        int qty  = qty_dist(rng);

        if (roll < t_rest) {
            // Resting limit near the touch.
            int price = cfg.mid_price + rest_off(rng);
            int id = next_id++;
            actions.push_back({Action::AddLimit,
                               Order::limit(id, buy, price, qty), 0});
            live_ids.push_back(id);
        } else if (roll < t_cross) {
            // Crossing limit: buy priced above mid, sell priced below mid.
            int price = buy ? cfg.mid_price + cross_off(rng)
                            : cfg.mid_price - cross_off(rng);
            int id = next_id++;
            actions.push_back({Action::AddLimit,
                               Order::limit(id, buy, price, qty), 0});
            live_ids.push_back(id);
        } else if (roll < t_market) {
            // Market order: walks the book, never rests, so not added to live_ids.
            int id = next_id++;
            actions.push_back({Action::AddMarket,
                               Order::market(id, buy, qty), 0});
        } else {
            // Cancel a previously-seen id (may already be filled => cancel misses,
            // which is itself a path worth exercising). If none yet, fall back to
            // a resting limit so early iterations aren't wasted.
            if (live_ids.empty()) {
                int price = cfg.mid_price + rest_off(rng);
                int id = next_id++;
                actions.push_back({Action::AddLimit,
                                   Order::limit(id, buy, price, qty), 0});
                live_ids.push_back(id);
            } else {
                std::uniform_int_distribution<size_t> pick(0, live_ids.size() - 1);
                int id = live_ids[pick(rng)];
                actions.push_back({Action::Cancel, Order::limit(0,false,0,0), id});
            }
        }
    }
    return actions;
}