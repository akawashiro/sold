#include <stdio.h>
#include <threads.h>

// thread_local int tls_bss_i;
static int global_x;
thread_local int tls_data_j = 3;

int main() {
    // tls_bss_i = 2;
    // printf("tls_bss_i = %d\n", tls_bss_i);
    global_x = 2;
    printf("global_x = %d\n", global_x);
    printf("tls_data_j = %d\n", tls_data_j);
    return 0;
}
