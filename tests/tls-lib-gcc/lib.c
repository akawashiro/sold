#include "lib.h"

int normal_l;
int return_tls_i() {
    normal_l = 10;
    return thread_local_i;
}
