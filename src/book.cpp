#include "book.h"
#include <algorithm>
#include <iterator>
#include <iostream>



Order Order :: limit(int id, bool buyside, int price, int qty){
    return Order{id, buyside, price, qty, false};
}

Order Order :: market(int id, bool buyside, int qty){
    return Order{id, buyside, 0, qty, true};
}



void Book:: matching(Order& newOrder, std::list<PriceLevel>& oppositeList,std::vector<Fill>& fills){
           
    while (!oppositeList.empty() && newOrder.qty_remaining>0){
        std::list<PriceLevel>::iterator it =oppositeList.begin(); // first price level
        std::list<Order>::iterator orderIt= it->orders.begin();  //first order in that price level
        
        int subtractedValue=std::min(newOrder.qty_remaining,orderIt->qty_remaining);
        newOrder.qty_remaining-=subtractedValue;
        orderIt->qty_remaining-=subtractedValue;
        fills.push_back({newOrder.id,orderIt->id,orderIt->price,subtractedValue});
        

        // either order is completed or qty at current price goes 0
        if (orderIt->qty_remaining==0){
            orderMap.erase(orderIt->id);
            it->orders.pop_front();
            if (it->orders.empty()){
                oppositeList.pop_front();
                
            }
        }
    }
}


bool Book::market_match(Order & newOrder, std::vector<Fill>& fills){
    std::list<PriceLevel>& oppositeList = newOrder.buyside ? sellSide : buySide;
    if(oppositeList.empty()){return false;}

    matching(newOrder,oppositeList,fills);
    
    return true;

}


std::vector<Fill> Book:: add(Order newOrder){
    std::list<PriceLevel> &correctList=(newOrder.buyside)? buySide:sellSide;
    std::vector<Fill> fills;

    auto insertToMap =[&](const auto& it,const bool& create,const Order& order){
        if (create){
            auto newLevel=correctList.insert(it,{order.price,{order}});
            orderMap[order.id]={newLevel,newLevel->orders.begin()};            
        }else{
            it->orders.emplace_back(order);
            orderMap[order.id]={it,std::prev(it->orders.end())};
        }
    };

    auto matchingPriceComparator= [](std::list<PriceLevel>& listToMatch,Order& order){
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


    auto limit_match=[&](){
        std::list<PriceLevel>& oppositeList = newOrder.buyside ? sellSide : buySide;

        while(!oppositeList.empty() && matchingPriceComparator(oppositeList,newOrder) && newOrder.qty_remaining>0){
            //orders at particular price
            matching(newOrder,oppositeList,fills);
        }

        if (newOrder.qty_remaining>0){ // we could not complete whole newOrder
            insertToList(newOrder);

        }

    };
    if(newOrder.marketOrder){
        market_match(newOrder,fills);
    }else{
        limit_match();
    }
    return fills;
}



bool Book::cancel(int id){
    auto it=orderMap.find(id);
    OrderLocation orderLoc;
    if (it==orderMap.end()){return false;}
    
    orderLoc=it->second;

    std::list<PriceLevel>::iterator priceLevelIt= orderLoc.priceLevel;
    std::list<Order>::iterator orderIt=orderLoc.order;

    std::list<PriceLevel> & correctSide= orderLoc.order->buyside? buySide:sellSide;

    priceLevelIt->orders.erase(orderIt);
    if (priceLevelIt->orders.empty()){
        correctSide.erase(priceLevelIt);
    }

    orderMap.erase(it);
    return true;
    
}

void Book:: print_book(){
    
    auto print=[](const std::list<PriceLevel>& printList,const bool& buy){
        if( buy){
            std::cout<<"buy side"<<"\n";
        }else{
            std::cout<<"sell side"<<"\n";
        }

        for(auto it=printList.begin(); it!=printList.end();it++){
            std::cout<<"price level is "<<it->price;
            const std::list<Order> &orders=it->orders;

            for (auto order= orders.begin();order!=orders.end();order++){
                std::cout<<" id: "<< order->id<< " price: "<<order->price<< " qty remaining: "<<order->qty_remaining;
                std::cout<<"\n";
            }

        }

    };
    print(buySide,true);
    std::cout<<"\n";
    std::cout<<"\n";
    print(sellSide,false);

}


int Book::best_bid() const{
    if(buySide.empty()){return -1;}
    return buySide.front().price;
}

int Book::best_ask()const{
    if(sellSide.empty()){return -1;}
    return sellSide.front().price;
}

int Book:: qty_at(bool buyside, int price)const{
    const std::list<PriceLevel>& correctList= buyside ? buySide:sellSide;
    auto it= std::find_if(correctList.begin(),correctList.end(),[price](const PriceLevel & priceLevel){
        return priceLevel.price==price;
    });

    if(it==correctList.end()){return -1;}

    const std::list<Order>& orders=it->orders;
    int totalQty=0;

    for(auto it =orders.begin();it!=orders.end();it++){
        totalQty+=it->qty_remaining;
    }
    return totalQty;



}


