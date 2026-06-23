#pragma once

#include <list>
#include <unordered_map>


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

struct OrderLocation{
    std::list<PriceLevel>::iterator priceLevel;
    std::list<Order>::iterator order;
};

class Book{

    private:
        std::list<PriceLevel> buySide, sellSide;
        std::unordered_map<int,OrderLocation> orderMap;

        void matching(Order& newOrder, std::list<PriceLevel>& oppositeList);

        bool market_match(Order & newOrder);
    
    public:
        void add(Order newOrder);

        bool cancel(int id);

        void print_book();

};
