#include <threads.h>

thread_local int i;
__thread int j = 3;

int return_tls_i() {
    return i;
}

int return_tls_j() {
    return j;
}
