#include "book.h"
#include <iostream>

int main(){
    // Test A: market buy on empty book
    {
        Book book;
        book.add(Order::market(1, true, 5));
        book.print_book();
    }
    // Test B: multi-order level sweep
    {
        Book book;
        book.add(Order::limit(10, false, 195, 6));
        book.add(Order::limit(13, false, 195, 3));
        book.add(Order::limit(11, false, 198, 9));
        book.add(Order::market(12, true, 12));
        book.print_book();
    }
    // ... B2 and C from earlier
    return 0;
}