#include <iostream>

class A {
public:
    virtual void Speak(){};
};

class B : public A {
public:
    void Speak() override;
    int a;
    int b;
};

class C : public A {
public:
    void Speak() override;
};

void JustHelloFun();
