#include "lib.h"

int normal_l = 11;
int return_tls_i() {
    return thread_local_i;
}
