#include <dlfcn.h>
#include <stdio.h>

int main() {
    void* handle = dlopen("lib.so", RTLD_LAZY);
    if (handle != NULL) {
        void (*func)() = (void (*)())dlsym(handle, "show_tls_variables");
        func();

        int* tls_with_init_value_p = (int*)dlsym(handle, "tls_with_init_value");
        int* tls_without_init_value_p = (int*)dlsym(handle, "tls_without_init_value");

        printf("*tls_with_init_value_p = %d, *tls_without_init_value_p = %d\n", *tls_with_init_value_p, *tls_without_init_value_p);

    } else {
        printf("Cannot open lib.so\n");
    }
    return 0;
}
