#include "fuga.h"

namespace {
thread_local uint64_t fuga_data = 0xDEADBEEFDEADBEEF;
thread_local uint64_t fuga_bss;
}  // namespace

uint64_t get_fuga_data() {
    return fuga_data;
}

uint64_t get_fuga_bss() {
    return fuga_bss;
}
