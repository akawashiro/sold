#include <stdio.h>
#include "lib.h"

int main() {
    printf("i = %d, j = %d\n", return_tls_i(), return_tls_j());
    return 0;
}
