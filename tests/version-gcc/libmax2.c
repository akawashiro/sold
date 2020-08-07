#include <stdio.h>

int max__1(int a, int b){
    printf("max__1 @ libmax2\n");
    return (a > b ? a : b);
}

int max__2(int a, int b, int c){
    printf("max__2 @ libmax2\n");
    int r = a > b ? a : b;
    r = r > c ? r : c;
    return r;
}

__asm__(".symver max__1,max@LIBMAX_1.0");
__asm__(".symver max__2,max@@LIBMAX_2.0");
