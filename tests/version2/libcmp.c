#include <stdio.h>

int cmp1(int a, int b){
    printf("cmp1\n");
    return (a > b ? a : b);
}

int cmp2(int a, int b){
    printf("cmp2\n");
    return (a < b ? a : b);
}

