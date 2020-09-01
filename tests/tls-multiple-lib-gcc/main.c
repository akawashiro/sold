#include <stdio.h>
#include "lib.h"

int main() {
    printf("i = %d\n", return_tls_i());
    thread_local_j = 3;
    printf("j = %d\n", thread_local_j);
    printf("k = %d\n", thread_local_k);
    printf("l = %d\n", thread_local_l);
    return 0;
}
