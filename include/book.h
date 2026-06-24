#pragma once

#include <list>
#include <unordered_map>
#include <vector>


struct Order{
    int id;
    bool buyside;
    int price;
    int qty_remaining;
    bool marketOrder;

    static Order limit(int id, bool buyside, int price, int qty);

    static Order market(int id, bool buyside, int qty);
};

struct PriceLevel{
    int price;
    std::list<Order> orders;
};

struct Fill{
    int taker_id;
    int maker_id;
    int price;
    int qty;
};

struct OrderLocation{
    std::list<PriceLevel>::iterator priceLevel;
    std::list<Order>::iterator order;
};

class Book{

    private:
        std::list<PriceLevel> buySide, sellSide;
        std::unordered_map<int,OrderLocation> orderMap;

        void matching(Order& newOrder, std::list<PriceLevel>& oppositeList,std::vector<Fill>& fills);

        bool market_match(Order & newOrder,std::vector<Fill>& fills);
    
    public:
        std::vector<Fill> add(Order newOrder);

        bool cancel(int id);

        void print_book();

        int best_bid() const;

        int best_ask() const;

        int qty_at(bool buyside, int price) const ;

};
