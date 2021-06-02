#include <cassert>

#include <iostream>

#include "fuga.h"
#include "hoge.h"

extern "C" void show_fuga_hoge() {
    uint64_t fuga_data = get_fuga_data();
    uint64_t fuga_bss = get_fuga_bss();
    uint64_t hoge_data = get_hoge_data();
    uint64_t hoge_bss = get_hoge_bss();

    std::cout << std::hex << "fuga_data = " << fuga_data << ", fuga_bss = " << fuga_bss << std::endl
              << "hoge_data = " << hoge_data << ", hoge_bss = " << hoge_bss << std::endl;

    assert(fuga_data == 0xDEADBEEFDEADBEEF && fuga_bss == 0 && hoge_data == 0xABCDEFABCDEFABCD && hoge_bss == 0);
}
