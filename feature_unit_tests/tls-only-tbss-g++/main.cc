#include <dlfcn.h>

#include <iostream>

int main() {
    std::cout << "---------- libfugahoge.so.original ----------" << std::endl;
    void* handle = dlopen("./libfugahoge.so.original", RTLD_LAZY);
    if (handle == NULL) {
        std::cout << "Cannot find libfugahoge.so.original" << std::endl;
        return 0;
    }
    int (*show_fuga_hoge)() = (int (*)())dlsym(handle, "show_fuga_hoge");
    if (show_fuga_hoge == NULL) {
        std::cout << "Cannot find show_fuga_hoge" << std::endl;
        return 0;
    }
    show_fuga_hoge();

    std::cout << "---------- libfugahoge.so.soldout ----------" << std::endl;
    handle = dlopen("./libfugahoge.so.soldout", RTLD_LAZY);
    if (handle == NULL) {
        std::cout << "Cannot find libfugahoge.so.soldout" << std::endl;
        return 0;
    }
    show_fuga_hoge = (int (*)())dlsym(handle, "show_fuga_hoge");
    if (show_fuga_hoge == NULL) {
        std::cout << "Cannot find show_fuga_hoge" << std::endl;
        return 0;
    }
    show_fuga_hoge();
    return 0;
}
