#include <stdio.h>

extern int hoge;

int main() {
    hoge = 10;
    printf("hoge = %d\n", hoge);
}
