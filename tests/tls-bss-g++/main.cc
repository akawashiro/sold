#include <stdio.h>
#include <threads.h>

thread_local int tls_bss_i;

int main() {
    tls_bss_i = 2;
    printf("tls_bss_i = %d\n", tls_bss_i);
    printf("tls_bss_i = %d\n", tls_bss_i + 10);
    return 0;
}
