#include <stdio.h>

int cmp__1(int a, int b){
    printf("cmp__1\n");
    return (a > b ? a : b);
}

int cmp__2(int a, int b){
    printf("cmp__2\n");
    return (a < b ? a : b);
}

__asm__(".symver cmp__1,cmp@LIBCMP_1.0");
__asm__(".symver cmp__2,cmp@@LIBCMP_2.0");
