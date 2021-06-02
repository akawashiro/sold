#include "lib.h"

int return_tls_i() {
    return thread_local_i;
}

int return_tls_j() {
    return thread_local_j;
}
