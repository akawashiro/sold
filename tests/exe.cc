#include <stdio.h>
#include <stdlib.h>

#include "lib.h"

static int g_init;
__attribute__((constructor)) static void init() {
    puts("Constructor in exe");
    g_init = 42;
}

__attribute__((destructor)) static void quit() {
    puts("Destructor in exe");
}

int main(int argc, char** argv) {
    int is_exe = (argc == 1);

    puts("Start main");
    if (lib_add_42_via_base(10) != 52) abort();
    if (in_both_lib_and_base() != 100) abort();

    fprintf(stderr, "Use stderr from main\n");
    lib_use_stderr();

    if (g_init != 42) abort();
    // TODO(hamaji): Fix .init_array for the main binary.
    if (!is_exe) {
        if (lib_init_value() != 43) abort();
        if (lib_base_init_value() != 44) abort();
        puts("Constructors are OK");
    }

    if (lib_base_global() != 98) abort();

    if (lib_base_vf() != 1234) abort();

    if (lib_base_thread_var() != 121) abort();
    if (lib_base_thread_var_reloc() != 42) abort();
    if (lib_thread_var() != 111) abort();
    if (lib_thread_var_reloc() != 32) abort();

    puts("OK");
}
