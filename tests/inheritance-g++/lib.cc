#include "lib.h"

void B::Speak() {
    std::cout << "I am B." << std::endl;
}

void C::Speak() {
    std::cout << "I am C." << std::endl;
}

void JustHelloFun() {
    std::cout << "Hello from lib" << std::endl;

    B* bptr = new B;
    C* cptr = new C;

    A* aptr = bptr;
    aptr->Speak();
    aptr = cptr;
    aptr->Speak();

    delete bptr;
    delete cptr;
}
