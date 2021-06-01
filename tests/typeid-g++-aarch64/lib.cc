#include <iostream>
#include <typeinfo>

class Base {
public:
    virtual void f() {}
};

class Derived : public Base {};

void PrintTypeid() {
    Base b;
    Derived d;
    Base* p1 = &b;
    Base* p2 = &d;

    std::cout << typeid(b).name() << "\n"
              << typeid(d).name() << "\n"
              << typeid(p1).name() << "\n"
              << typeid(*p1).name() << "\n"
              << typeid(p2).name() << "\n"
              << typeid(*p2).name() << std::endl;
}
