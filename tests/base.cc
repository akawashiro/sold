#include "base.h"

#include <stdio.h>
#include <stdlib.h>

static int g_init;
__attribute__((constructor)) static void init() {
    puts("Constructor in base");
    g_init = 44;
}

__attribute__((destructor)) static void quit() {
    puts("Destructor in base");
}

int base_init_value() {
    return g_init;
}

int base_42() {
    printf("called %s\n", __func__);
    return 42;
}

int base_add_42(int x) {
    printf("called %s\n", __func__);
    return x + 42;
}

int in_both_lib_and_base() {
    fprintf(stderr, "Must have been overriden\n");
    abort();
}

void base_use_stderr() {
    fprintf(stderr, "Use stderr from base\n");
}

int base_global = 98;

class BaseImpl : public Base {
public:
    int vf() override { return 1234; }
};

Base* MakeBase() {
    return new BaseImpl();
}

thread_local int g_base_thread_var = 190;
thread_local int g_base_thread_var2 = 70;
thread_local int g_base_thread_bss;
thread_local int g_base_thread_bss2;
static int g_buf[] = {42};
thread_local int* g_base_thread_buf = g_buf;

int base_thread_var() {
    return (g_base_thread_var - g_base_thread_var2 - --g_base_thread_bss) * ++g_base_thread_bss2;
}

int base_thread_var_reloc() {
    return g_base_thread_buf[0];
}
