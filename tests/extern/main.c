#include <assert.h>
#include <stdio.h>

extern int hoge_var;
void inc();

int main() {
    printf("hoge_var = %x &hoge_var = %x\n", hoge_var, &hoge_var);
    assert(hoge_var == 0xdeadbeef);
    inc();
    printf("hoge_var = %x &hoge_var = %x\n", hoge_var, &hoge_var);
    assert(hoge_var == 0xdeadbeef + 1);
    hoge_var = 3;
    return 0;
}
