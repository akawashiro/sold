#include <stdio.h>
#include "lib.h"

int main() {
    printf("i = %d\n", return_tls_i());
    printf("l = %d\n", thread_local_l);
    return 0;
}
