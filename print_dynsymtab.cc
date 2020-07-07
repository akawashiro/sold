#include "elf_binary.h"

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        LOGF("Usage: %s <in-elf>\n", argv[0]);
        return 1;
    }

    auto b = ReadELF(argv[1]);
    b->ReadDynSymtab();
    b->ParseVerneeds();
    b->ParseVersyms();
}
