#ifndef REFERENCE_BOOK_H
#define REFERENCE_BOOK_H
#include <vector>
#include "book.h"   // reuse Order and Fill so both engines speak the same types

// Deliberately slow, deliberately obvious. A flat vector of resting orders
// scanned linearly on every action. No pool, no intrusive list, no maps.
// The whole point is independent correctness: if this and Book ever disagree
// on a fill stream, one of them is wrong.
class ReferenceBook {
private:
    std::vector<Order> resting;   // index order == arrival order == time priority

public:
    // Match incoming against the book, return the fills produced by THIS action.
    std::vector<Fill> add(Order incoming);

    // Remove the order with this id if present. true if erased, false on miss.
    bool cancel(int id);
};

#endif
