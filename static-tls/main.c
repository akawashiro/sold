#include <stdio.h>
#include <dlfcn.h>

void try_dlopen(const char* lib_path) {
    printf("--- Loading %s ---\n", lib_path);

    void* handle = dlopen(lib_path, RTLD_LAZY);

    if (!handle) {
        printf("Failed to load library.\ndlerror(): %s\n", dlerror());
    } else {
        printf("Successfully loaded library.\n");
        dlclose(handle);
    }
}

int main() {
    printf("Attempting to dynamically load shared libraries with large TLS blocks...\n\n");
    try_dlopen("./libfail.so");
    return 0;
}
