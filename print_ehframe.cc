//
// print_ehframe
//
// This program shows .eh_frame_hdr
//

#include "elf_binary.h"

#include <iostream>

int main(int argc, const char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <in-elf>\nThis program shows .eh_frame_hdr." << std::endl;
        return 1;
    }

    auto b = ReadELF(argv[1]);
    std::cout << b->ShowEHFrame();
    return 0;
}
