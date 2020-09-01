#include "lib.h"

__thread int thread_local_k = 42;

int return_tls_i() {
    return thread_local_i;
}

int return_tls_l() {
    return thread_local_l;
}

