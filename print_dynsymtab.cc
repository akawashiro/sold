//
// print_dynsymtab
//
// This program parses and prints DT_SYMTAB of a given ELF file. It prints all symbols
// and version information. When the ELF file does not include version information,
// it prints NO_VERSION_INFO instead of version information.
//
// Unlike nm -D --with-symbol-versions or readelf --dynsyms, it works without any shdrs
// because print_dynsymtab gets all information only from phdrs.

#include "elf_binary.h"

#include <iostream>

#include "sold.h"

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <in-elf>\nThis program parse DT_SYMTAB of the given ELF file." << std::endl;
        return 1;
    }

    // This Sold class instrance is used only to get filename_to_soname map.
    Sold sold(argv[1], {}, false);

    auto b = ReadELF(argv[1]);
    b->ReadDynSymtab(sold.filename_to_soname());
    std::cout << b->ShowDynSymtab();
}
