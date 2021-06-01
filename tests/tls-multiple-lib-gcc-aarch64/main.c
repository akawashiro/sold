#include <stdio.h>
#include "lib.h"

int main() {
    printf("i = %d\n", return_tls_i());
    printf("j = %d\n", return_tls_j());
    return 0;
}
