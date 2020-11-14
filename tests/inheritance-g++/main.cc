#include "lib.h"

#include <iostream>

void CallSpeak(A* aptr) {
    aptr->Speak();
}

int main() {
    std::cout << "Before JustHelloFun" << std::endl;
    JustHelloFun();
    std::cout << "After JustHelloFun\n" << std::endl;

    std::cout << "Before constructors" << std::endl;
    B b;
    C c;
    std::cout << "After constructors\n" << std::endl;

    std::cout << "Before member functions" << std::endl;
    b.Speak();
    c.Speak();
    std::cout << "After member functions\n" << std::endl;

    std::cout << "Before CallSpeak" << std::endl;
    CallSpeak(&b);
    CallSpeak(&c);
    std::cout << "After CallSpeak" << std::endl;
    return 0;
}
