#include <map>
#include <string>
#include <vector>

#include "utils.h"

class ShdrBuilder {
public:
    enum ShdrType { GnuHash, Dynsym, GnuVersion, GnuVersionR, Dynstr, RelaDyn, Init, Strtab, Shstrtab, Dynamic, Text, TLS };
    void EmitShstrtab(FILE* fp);
    void EmitShdrs(FILE* fp);
    uintptr_t ShstrtabSize() const;
    Elf_Half CountShdrs() const { return shdrs.size(); }
    void RegisterShdr(Elf_Off offset, uint64_t size, ShdrType type);
    // The index of shstrndx is fixed 0.
    Elf_Half Shstrndx() const { return 0; }

private:
    const std::map<ShdrType, std::string> type_to_str = {
        {GnuHash, ".gnu.hash"}, {Dynsym, ".dynsym"}, {GnuVersion, ".gnu.version"}, {GnuVersionR, ".gnu.version_r"}, {Dynstr, ".dynstr"},
        {RelaDyn, ".rela.dyn"}, {Init, ".init"},     {Strtab, ".strtab"},          {Shstrtab, ".shstrtab"},         {Dynamic, ".dynamic"},
        {Text, ".text"},        {TLS, ".tls"}};
    std::vector<Elf_Shdr> shdrs;
    uint32_t ShstrtabIndex(ShdrType type) const;
};
