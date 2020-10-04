#include <stdio.h>

__thread int tls_bss_i;
__thread int tls_data_j = 3;

int main() {
    printf("tls_data_j = %d\n", tls_data_j);
    tls_bss_i = 2;
    printf("tls_bss_i = %d\n", tls_bss_i);
    return 0;
}
