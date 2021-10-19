#include <assert.h>

extern int hoge_var;

int main() {
    assert(hoge_var == 0xdeadbeef);
    hoge_var = 3;
    return 0;
}
