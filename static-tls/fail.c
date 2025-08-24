#include <stdio.h>
#include <threads.h>

static thread_local char large_tls[1024 * 1024 * 1024] = {0xFF};

void dummy_function() {
    printf("This is a dummy function in libfail.so\n");
}
