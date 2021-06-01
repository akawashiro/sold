#include <iostream>
#include <typeinfo>

class Base {
public:
    virtual ~Base() {}
};
class Sub1 : public Base {};
class Sub2 : public Base {};

void TryDowncast() {
    Base* base = new Sub1;
    Sub2* sub2 = dynamic_cast<Sub2*>(base);
    if (sub2 == nullptr) {
        std::cout << "Downcast failed" << std::endl;
    }
}
