#include <map>
#include <string>
#include <vector>

#include "utils.h"

class ShdrBuilder {
public:
    enum ShdrType { GnuHash, Dynsym, GnuVersion, GnuVersionR, Dynstr, RelaDyn, Init, Fini, Strtab, Shstrtab, Dynamic, Text, TLS };
    void EmitShstrtab(FILE* fp);
    void EmitShdrs(FILE* fp);
    uintptr_t ShstrtabSize() const;
    Elf_Half CountShdrs() const { return shdrs.size(); }
    void RegisterShdr(Elf_Off offset, uint64_t size, ShdrType type, uint64_t entsize = 0);
    Elf_Half Shstrndx() const { return shdrs.size() - 1; }

    // After register all shdrs, you must call Freeze.
    void Freeze();

private:
    const std::map<ShdrType, std::string> type_to_str = {{GnuHash, ".gnu.hash"},
                                                         {Dynsym, ".dynsym"},
                                                         {GnuVersion, ".gnu.version"},
                                                         {GnuVersionR, ".gnu.version_r"},
                                                         {Dynstr, ".dynstr"},
                                                         {RelaDyn, ".rela.dyn"},
                                                         {Init, ".init"},
                                                         {Fini, ".fini"},
                                                         {Strtab, ".strtab"},
                                                         {Shstrtab, ".shstrtab"},
                                                         {Dynamic, ".dynamic"},
                                                         {Text, ".text"},
                                                         {TLS, ".tls"}};

    // The first section header must be NULL.
    std::vector<Elf_Shdr> shdrs = {Elf_Shdr{0}};
    uint32_t GetShName(ShdrType type) const;
    uint32_t GetIndex(ShdrType type) const;
};
