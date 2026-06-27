#include "book.h"

int main(){
    Book book(1024);                            // pool capacity — pick a size ≥ peak resting orders
    book.add(Order::limit(30, false, 200, 10)); // ask 200: id30 x10
    book.add(Order::market(31, true, 4));       // market BUY 4
    book.print_book();
}