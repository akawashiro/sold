#include "hoge.h"

namespace {
thread_local uint64_t hoge_data = 0xABCDEFABCDEFABCD;
thread_local uint64_t hoge_bss;
}  // namespace

uint64_t get_hoge_data() {
    return hoge_data;
}

uint64_t get_hoge_bss() {
    return hoge_bss;
}
