#ifndef BOOK_H
#define BOOK_H
#include <list>
#include <unordered_map>
#include <vector>



struct Order{
    int id;
    bool buyside;
    int price;
    int qty_remaining;
    bool marketOrder;

    Order* next;
    Order* prev;

    static Order limit(int id, bool buyside, int price, int qty);

    static Order market(int id, bool buyside, int qty);
};
struct OrderPool{
    Order * storage;
    Order * free_head;
    int capacity;

    OrderPool(int cap);
    ~OrderPool();

    Order* acquire();
    void release(Order* o);
};

struct PriceLevel{
    int price;
    Order* head;
    Order* tail;
};

struct Fill{
    int taker_id;
    int maker_id;
    int price;
    int qty;

    bool operator==(const Fill& o) const {   // body inside the struct = inline, header-safe
        return taker_id == o.taker_id
            && maker_id == o.maker_id
            && price    == o.price
            && qty      == o.qty;
    }
};

struct OrderLocation{
    std::list<PriceLevel>::iterator priceLevel;
    Order* order;
};

class Book{

    private:
        std::list<PriceLevel> buySide, sellSide;
        std::unordered_map<int,OrderLocation> orderMap;
        OrderPool pool;

        void matching(Order& newOrder, std::list<PriceLevel>& oppositeList,std::vector<Fill>& fills);

        bool market_match(Order & newOrder,std::vector<Fill>& fills);
    
    public:
        std::vector<Fill> add(Order newOrder);

        Book(int cap);

        bool cancel(int id);

        void print_book();

        int best_bid() const;

        int best_ask() const;

        int qty_at(bool buyside, int price) const ;

};

#endif