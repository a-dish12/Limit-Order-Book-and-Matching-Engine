#include "reference_book.h"
#include <algorithm>   // std::min

std::vector<Fill> ReferenceBook::add(Order incoming) {
    std::vector<Fill> fills;

    while (incoming.qty_remaining > 0) {
        // Find the best eligible resting order on the OPPOSITE side.
        // Opposite side: a buy trades against resting sells, a sell against
        // resting buys. Eligibility: a buy needs ask <= its price, a sell
        // needs bid >= its price. A market order ignores price entirely.
        // "Best" = best price for the incoming side; ties broken by oldest,
        // i.e. lowest index (price-time priority).
        int best = -1;
        for (int i = 0; i < (int)resting.size(); ++i) {
            const Order& r = resting[i];
            if (r.buyside == incoming.buyside) continue;   // same side, skip

            if (!incoming.marketOrder) {
                if (incoming.buyside && r.price > incoming.price) continue;   // ask too high
                if (!incoming.buyside && r.price < incoming.price) continue;  // bid too low
            }

            if (best == -1) {
                best = i;
                continue;
            }

            // Better price wins; equal price keeps the older one (already at
            // a lower index, so we only replace on a strictly better price).
            if (incoming.buyside) {
                if (r.price < resting[best].price) best = i;   // buyer wants lowest ask
            } else {
                if (r.price > resting[best].price) best = i;   // seller wants highest bid
            }
        }

        if (best == -1) break;   // nothing left to trade with

        Order& r = resting[best];
        int qty = std::min(incoming.qty_remaining, r.qty_remaining);
        fills.push_back(Fill{incoming.id, r.id, r.price, qty});
        incoming.qty_remaining -= qty;
        r.qty_remaining -= qty;

        if (r.qty_remaining == 0) {
            resting.erase(resting.begin() + best);
        }
    }

    // Leftover limit quantity rests; a market order with leftover just dies.
    if (incoming.qty_remaining > 0 && !incoming.marketOrder) {
        resting.push_back(incoming);
    }

    return fills;
}

bool ReferenceBook::cancel(int id) {
    for (int i = 0; i < (int)resting.size(); ++i) {
        if (resting[i].id == id) {
            resting.erase(resting.begin() + i);
            return true;
        }
    }
    return false;   // cancel-miss is legal, not an error
}
