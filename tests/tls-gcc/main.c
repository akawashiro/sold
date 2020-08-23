#include <stdio.h>
#include <threads.h>

thread_local int i;
thread_local int j = 3;

int main() {
    i = 2;
    printf("i = %d, j = %d\n", i, j);
    return 0;
}
