#include "book.h"
#include <algorithm>
#include <iterator>
#include <iostream>




Order Order :: limit(int id, bool buyside, int price, int qty){
    return Order{id, buyside, price, qty, false,nullptr,nullptr};
}

Order Order :: market(int id, bool buyside, int qty){
    return Order{id, buyside, 0, qty, true,nullptr,nullptr};
}

OrderPool:: OrderPool(int cap){
    capacity=cap;
    storage= new Order[capacity];

    for(int i=0;i<cap-1;i++){
        storage[i].next=&storage[i+1];
        storage[i].prev=nullptr;
    }

    storage[cap-1].next=nullptr;
    storage[cap-1].prev=nullptr;

    free_head=&storage[0];
}

OrderPool ::~OrderPool(){
    delete[] storage;
}

Order *OrderPool :: acquire(){
    if(free_head==nullptr){return nullptr;}

    Order* slot=free_head;
    free_head=slot->next;

    slot->next=nullptr;
    return slot;
}

void OrderPool ::release(Order* o){
    o->prev=nullptr;
    o->next=free_head;
    free_head=o;
}


Book:: Book(int cap):pool(cap){}

void Book:: matching(Order& newOrder, std::list<PriceLevel>& oppositeList,std::vector<Fill>& fills){

    auto matchingPriceComparator= [& newOrder, & oppositeList](){
        if (newOrder.buyside){
            if(oppositeList.front().price<=newOrder.price){
                return true;
            }
        }else{
            if(oppositeList.front().price>=newOrder.price){
                return true;
            }
        }
        return false;
    };
           
    while (!oppositeList.empty() && newOrder.qty_remaining>0){
        if(!newOrder.marketOrder && !matchingPriceComparator()){
            break;
        }
        std::list<PriceLevel>::iterator it =oppositeList.begin(); // first price level
        // std::list<Order>::iterator orderIt= it->orders.begin();  //first order in that price level
        Order * frontOrder=it->head;

        
        int subtractedValue=std::min(newOrder.qty_remaining,frontOrder->qty_remaining);
        newOrder.qty_remaining-=subtractedValue;
        frontOrder->qty_remaining-=subtractedValue;
        fills.push_back({newOrder.id,frontOrder->id,frontOrder->price,subtractedValue});
        

        // either order is completed or qty at current price goes 0
        if (frontOrder->qty_remaining==0){
            orderMap.erase(frontOrder->id);

            Order * nextOrder=frontOrder->next;

            if(nextOrder==nullptr){// list of orders now empty, pop that price level
                oppositeList.pop_front();
            }else{ // we still have more orders
                it->head=nextOrder;
                nextOrder->prev=nullptr;
                
            }
            pool.release(frontOrder);
            
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
   

    auto insertToList = [&]( Order& order) {
        auto it = std::find_if(correctList.begin(), correctList.end(),
            [&](const PriceLevel& p) { 
                if (order.buyside){
                    return order.price>=p.price;
                }else{
                    return order.price<=p.price;
                }

                });

        Order *slot=pool.acquire();
        slot->id=order.id;
        slot->buyside=order.buyside;
        slot->marketOrder=order.marketOrder;
        slot->price=order.price;
        slot->qty_remaining=order.qty_remaining;
        slot->next=nullptr;
        slot->prev=nullptr;

        std::list<PriceLevel>::iterator pId;
       
        
        auto createNewPriceLevel=[](Order* slot){
            PriceLevel newLevel;
            newLevel.price=slot->price;
            newLevel.head=slot;
            newLevel.tail=slot;
            return newLevel;
        };

        if (it==correctList.end()){
            correctList.emplace_back(createNewPriceLevel(slot));
            pId=std::prev(correctList.end());    
        }else if(it->price==slot->price){
            slot->prev=it->tail;
            it->tail->next=slot;
            it->tail=slot;
            pId=it;
        }else{
            pId=correctList.insert(it,createNewPriceLevel(slot));
        }
        orderMap[order.id]={pId,slot};

    };


    auto limit_match=[&](){
        std::list<PriceLevel>& oppositeList = newOrder.buyside ? sellSide : buySide;

        matching(newOrder,oppositeList,fills);
        

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
    Order* orderP=orderLoc.order;

    std::list<PriceLevel> & correctSide= orderLoc.order->buyside? buySide:sellSide;

    if(orderP==priceLevelIt->head && orderP==priceLevelIt->tail){
        priceLevelIt->head=nullptr;
        priceLevelIt->tail=nullptr;
        correctSide.erase(priceLevelIt);
    }else if (orderP==priceLevelIt->head){
        priceLevelIt->head=priceLevelIt->head->next;
        priceLevelIt->head->prev=nullptr;
    }else if(orderP==priceLevelIt->tail){
        priceLevelIt->tail=orderP->prev;
        orderP->prev->next=nullptr;
    }else{
        orderP->prev->next=orderP->next;
        orderP->next->prev=orderP->prev;
    }

    orderMap.erase(id);
    pool.release(orderP);
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
            Order* order=it->head;

            while(order!=nullptr){
                std::cout<<" id: "<< order->id<< " price: "<<order->price<< " qty remaining: "<<order->qty_remaining;
                std::cout<<"\n";
                order=order->next;
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

    Order *order=it->head;
    int totalQty=0;

    while(order!=nullptr){
        totalQty+=order->qty_remaining;
        order=order->next;
    }
    return totalQty;
}