//
// print_arrays
//
// This program shows INIT_ARRAY and FINI_ARRAY
//

#include "elf_binary.h"

#include <iostream>

int main(int argc, const char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <in-elf>\nThis program shows INIT_ARRAY and FINI_ARRAY of the given ELF file." << std::endl;
        return 1;
    }

    auto b = ReadELF(argv[1]);
    std::cout << "INIT_ARRAY:" << std::endl;
    for (const uintptr_t i : b->init_array()) {
        std::cout << HexString(i) << std::endl;
    }
    std::cout << "FINI_ARRAY:" << std::endl;
    for (const uintptr_t f : b->fini_array()) {
        std::cout << HexString(f) << std::endl;
    }
    return 0;
}
