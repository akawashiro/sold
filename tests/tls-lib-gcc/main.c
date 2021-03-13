#include <stdio.h>
#include "lib.h"

int main() {
    printf("i = %d\n", return_tls_i());
    thread_local_j = 5;
    printf("j = %d\n", thread_local_j);
    // normal_k = 7;
    // printf("k = %d\n", normal_k);
    normal_l = 9;
    printf("l = %d\n", normal_l);
    return 0;
}
