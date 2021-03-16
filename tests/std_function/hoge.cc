#include "hoge.h"

struct YYY : public XXX {
    YYY(int n) : n_(n){};
    int n_;
    int get() { return 42; };
};

static std::unique_ptr<XXX> u_xxx(new YYY(42));

std::unique_ptr<XXX> GenXXX(int n) {
    static XXX* p = new YYY(n);
    return std::unique_ptr<XXX>(p);
}

void CreateXXX() {
    std::function<std::unique_ptr<XXX>(int)> f = GenXXX;
    std::unique_ptr<XXX> xxx = f(10);
    printf("%d\n", xxx.release()->get());
    printf("%d\n", u_xxx.release()->get());
}
