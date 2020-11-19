#include "lib.h"

#include <iostream>
#include <memory>

void JustHelloFunMain() {
    std::cout << "Hello from main" << std::endl;

    B* bptr = new B;
    C* cptr = new C;

    A* aptr = bptr;
    aptr->Speak();
    aptr = cptr;
    aptr->Speak();

    delete bptr;
    delete cptr;

    std::unique_ptr<A> buptr = std::make_unique<B>();
    std::unique_ptr<A> cuptr = std::make_unique<C>();
    buptr->Speak();
    cuptr->Speak();

    std::shared_ptr<A> bsptr = std::make_unique<B>();
    std::shared_ptr<A> csptr = std::make_unique<C>();
    bsptr->Speak();
    csptr->Speak();
}

void CallSpeak(A* aptr) {
    aptr->Speak();
}

int main() {
    std::cout << "Before JustHelloFun" << std::endl;
    JustHelloFun();
    std::cout << "After JustHelloFun\n" << std::endl;

    std::cout << "Before JustHelloFunMain" << std::endl;
    JustHelloFunMain();
    std::cout << "After JustHelloFunMain\n" << std::endl;

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
