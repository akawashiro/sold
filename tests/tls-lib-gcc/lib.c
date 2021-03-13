#include "lib.h"

int normal_l;
int return_tls_i() {
    return thread_local_i;
}
