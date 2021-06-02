#include <stdio.h>

__thread int tls_with_init_value = 0;
__thread int tls_without_init_value;

void show_tls_variables() {
    tls_with_init_value = 10;
    tls_without_init_value = 10;
    printf("tls_with_init_value = %d, tls_without_init_value = %d\n", tls_with_init_value, tls_without_init_value);
}
