#include <iostream>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <iterator>
using namespace std;

struct Order{
    int id;
    bool buyside;
    int price;
    int qty_remaining;
};

struct PriceLevel{
    int price;
    list<Order> orders;
};

struct OrderLocation{
    list<PriceLevel>::iterator priceLevel;
    list<Order>::iterator order;
};

class Book{
    private:
        list<PriceLevel> buySide, sellSide;
        unordered_map<int,OrderLocation> orderMap;

    public:
        void add(Order newOrder){
            list<PriceLevel> &correctList=(newOrder.buyside)? buySide:sellSide;

            auto insertToMap =[&](const auto& it,const bool& create,const Order& order){
                if (create){
                    auto newLevel=correctList.insert(it,{order.price,{order}});
                    orderMap[order.id]={newLevel,newLevel->orders.begin()};            
                }else{
                    it->orders.emplace_back(order);
                    orderMap[order.id]={it,std::prev(it->orders.end())};
                }
            };
            auto matchingPriceComparator= [](list<PriceLevel>& listToMatch,Order& order){
                if (order.buyside){
                    if(listToMatch.front().price<=order.price){
                        return true;
                    }
                }else{
                    if(listToMatch.front().price>=order.price){
                        return true;
                    }
                }
                return false;
            };

            auto insertToList = [&](const Order& order) {
                auto it = std::find_if(correctList.begin(), correctList.end(),
                    [&](const PriceLevel& p) { 
                        if (order.buyside){
                            return order.price>=p.price;
                        }else{
                            return order.price<=p.price;
                        }

                     });
                if (it==correctList.end()){
                    insertToMap(it,true,order);                   
                }else if (it->price==order.price){
                    insertToMap(it,false,order);
                }else{
                   insertToMap(it,true,order);
                }
            };


            auto match=[&](){
                list<PriceLevel>& oppositeList = newOrder.buyside ? sellSide : buySide;

                while(!oppositeList.empty() && matchingPriceComparator(oppositeList,newOrder) && newOrder.qty_remaining>0){

                    //orders at particular price
                    list<Order>& priceLevelOrders=oppositeList.front().orders;
                    while (!priceLevelOrders.empty() && newOrder.qty_remaining>0){
                        Order & frontOrder=priceLevelOrders.front();

                        if (newOrder.qty_remaining>0){
                            int subtractedValue=min(newOrder.qty_remaining,frontOrder.qty_remaining);
                            newOrder.qty_remaining-=subtractedValue;
                            frontOrder.qty_remaining-=subtractedValue;
                            

                            // either order is completed or qty at current price goes 0
                            if (frontOrder.qty_remaining==0){
                                orderMap.erase(frontOrder.id);
                                priceLevelOrders.pop_front();
                                if (priceLevelOrders.empty()){
                                    oppositeList.pop_front();
                                }

                            }
                            if (newOrder.qty_remaining==0){
                                break;
                            }
                        }
                    }
                }

                if (newOrder.qty_remaining>0){ // we could not complete whole newOrder
                    insertToList(newOrder);

                }

            };
            match();
        }

        bool cancel(int id){
            auto it=orderMap.find(id);
            OrderLocation orderLoc;
            if (it==orderMap.end()){return false;}
            
            orderLoc=it->second;

            list<PriceLevel>::iterator priceLevelIt= orderLoc.priceLevel;
            list<Order>::iterator orderIt=orderLoc.order;

            list<PriceLevel> & correctSide= orderLoc.order->buyside? buySide:sellSide;

            priceLevelIt->orders.erase(orderIt);
            if (priceLevelIt->orders.empty()){
                correctSide.erase(priceLevelIt);
            }

            orderMap.erase(it);
            return true;
            
        }

        void print_book(){
            
            auto print=[](const list<PriceLevel>& printList,const bool& buy){
                if( buy){
                    cout<<"buy side"<<"\n";
                }else{
                    cout<<"sell side"<<"\n";
                }

                for(auto it=printList.begin(); it!=printList.end();it++){
                    cout<<"price level is "<<it->price;
                    const list<Order> &orders=it->orders;

                    for (auto order= orders.begin();order!=orders.end();order++){
                        cout<<" id: "<< order->id<< " price: "<<order->price<< " qty remaining: "<<order->qty_remaining;
                        cout<<"\n";
                    }

                }

            };
            print(buySide,true);
            cout<<"\n";
            cout<<"\n";
            print(sellSide,false);

        }


};


int main (){
    Book book;

    // ---- BUY SIDE (must end up DESCENDING by price, FIFO within a level) ----
    book.add({1, true, 100, 10});   // first level at 100
    book.add({2, true, 100, 5});    // SAME price -> must queue BEHIND order 1 (FIFO)
    book.add({3, true, 105, 8});    // NEW BEST (higher) -> must land at FRONT
    book.add({4, true, 102, 7});    // BETWEEN 105 and 100 -> must slot in the middle
    


    // ---- SELL SIDE (must end up ASCENDING by price, FIFO within a level) ----
    book.add({5, false, 200, 10});  // first level at 200
    book.add({6, false, 200, 4});   // SAME price -> must queue BEHIND order 5 (FIFO)
    book.add({7, false, 195, 6});   // NEW BEST (lower) -> must land at FRONT
    book.add({8, false, 198, 9});   // BETWEEN 195 and 200 -> must slot in the middle

    book.add({9, true, 200, 12});   // buy at 200: eats ask 195 (id7, 6), then ask 198 (id8, 9) partial, rests 0? hand-check it

    book.cancel(2);   // middle of FIFO at 100 — level survives, id1 stays, id2 gone
    book.cancel(3);   // only order at 105 — level 105 must vanish entirely
    book.cancel(8);   // partially-filled resting order (qty 3) — gone, level 198 vanishes
    book.cancel(999); // non-existent — returns false, book unchanged, no crash

    book.print_book();
    return 0;

    

    
}

