#include <functional>
#include <memory>

struct XXX {
    XXX(int n) : n_(n){};
    int n_;
};

static std::unique_ptr<XXX> = new XXX(42);

static std::unique_ptr<XXX> GenXXX(int n) {
    // static XXX* p = new XXX(n);
    return std::unique_ptr<XXX>(&xxx_one);
}

void CreateXXX() {
    std::function<std::unique_ptr<XXX>(int)> f = GenXXX;
    std::unique_ptr<XXX> xxx = f(10);
    printf("%d\n", xxx.release()->n_);
    // printf("%d\n", xxx_one.n_);
}
