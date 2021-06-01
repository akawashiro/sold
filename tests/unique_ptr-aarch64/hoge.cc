#include <cstdio>
#include <memory>

struct X {
    X(int v) : var(v){};
    int var;
    int fn() { return var; }
};

void fn() {
    auto up = std::make_unique<X>(42);
    auto p = up.release();
    printf("%d\n", p->fn());
}
