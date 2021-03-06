#include "lib.h"

#include <stdio.h>

#include <memory>

#include "base.h"

static int g_init;
__attribute__((constructor)) static void init() {
    puts("Constructor in lib");
    g_init = 43;
}

__attribute__((destructor)) static void quit() {
    puts("Destructor in lib");
}

int lib_init_value() {
    return g_init;
}

int lib_base_init_value() {
    return base_init_value();
}

int lib_42() {
    printf("called %s\n", __func__);
    return 42;
}

int lib_add_42_via_base(int v) {
    printf("called %s\n", __func__);
    return base_add_42(v);
}

int in_both_lib_and_base() {
    printf("called %s\n", __func__);
    return 100;
}

void lib_use_stderr() {
    fprintf(stderr, "Use stderr from lib\n");
}

int lib_base_global() {
    return base_global;
}

int* lib_base_global_ptr() {
    return &base_global;
}

int lib_base_vf() {
    printf("called %s\n", __func__);
    std::unique_ptr<Base> base(MakeBase());
    return base->vf();
}

int lib_base_thread_var() {
    return base_thread_var();
}

int lib_base_thread_var_reloc() {
    return base_thread_var_reloc();
}

thread_local int g_lib_thread_var = 180;
thread_local int g_lib_thread_var2 = 70;
thread_local int g_lib_thread_bss;
thread_local int g_lib_thread_bss2;
static int g_buf[] = {32};
thread_local int* g_lib_thread_buf = g_buf;

int lib_thread_var() {
    return (g_lib_thread_var - g_lib_thread_var2 - --g_lib_thread_bss) * ++g_lib_thread_bss2;
}

int lib_thread_var_reloc() {
    return g_lib_thread_buf[0];
}
