#include "hoge.h"

static XXX* p_xxx = nullptr;

void fuga() {
    CreateXXX();
    std::function<std::unique_ptr<XXX>(int)> f = GenXXX;
    std::unique_ptr<XXX> xxx = f(10);
    p_xxx = xxx.release();
    printf("%s\n", typeid(*p_xxx).name());
}
