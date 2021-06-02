#include <cstdio>

extern thread_local int hoge;
extern thread_local int fuga;

int main() {
    hoge = 42;
    fuga = 10;
    printf("hoge = %d, fuga = %d\n", hoge, fuga);
}
