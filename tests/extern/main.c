#include <assert.h>
#include <stdio.h>

extern int hoge_var;
int fuga_var = 0xaaaaaaaa;
void inc();

int main() {
    printf("hoge_var = %x &hoge_var = %x\n", hoge_var, &hoge_var);
    printf("hoge_var = %x &hoge_var = %x\n", *((&hoge_var - 1)), (&hoge_var - 1));
    printf("hoge_var = %x &hoge_var = %x\n", *((&hoge_var - 2)), (&hoge_var - 2));
    assert(hoge_var == 0xdeadbeef);
    inc();
    printf("hoge_var = %x &hoge_var = %x\n", hoge_var, &hoge_var);
    assert(hoge_var == 0xdeadbeef + 1);
    hoge_var = 3;
    return 0;
}
