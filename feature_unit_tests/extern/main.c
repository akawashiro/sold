#include <assert.h>
#include <stdio.h>

extern int hoge_var;
int fuga_var = 0xaaaaaaaa;
void inc();

int main() {
    printf("%s:%d hoge_var = %x &hoge_var = %x\n", __FILE__, __LINE__, hoge_var, &hoge_var);
    printf("%s:%d *((&hoge_var - 1)) = %x &(&hoge_var - 1) = %x\n", __FILE__, __LINE__, *((&hoge_var - 1)), (&hoge_var - 1));
    printf("%s:%d *((&hoge_var - 2)) = %x &(&hoge_var - 2) = %x\n", __FILE__, __LINE__, *((&hoge_var - 2)), (&hoge_var - 2));
    assert(hoge_var == 0xdeadbeef);
    inc();
    printf("%s:%d hoge_var = %x &hoge_var = %x\n", __FILE__, __LINE__, hoge_var, &hoge_var);
    assert(hoge_var == 0xdeadbeef + 1);
    return 0;
}
