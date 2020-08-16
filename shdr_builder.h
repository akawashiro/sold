#include <map>
#include <string>
#include <vector>

class ShdrBuilder {
public:
    enum ShdrType { GnuHash, Dynsym, GnuVersion, GnuVersionR, Dynstr, RelaDyn, Init, Strtab, Shstrtab, Dynmaic, Text, TLS };
    void EmitShstrtab(FILE* fp);
    uintptr_t SizeOfShstrtab() const;

private:
    const std::map<ShdrType, std::string> type_to_str = {
        {GnuHash, ".gnu.hash"}, {Dynsym, ".dynsym"}, {GnuVersion, ".gnu.version"}, {GnuVersionR, ".gnu.version_r"}, {Dynstr, ".dynstr"},
        {RelaDyn, ".rela.dyn"}, {Init, ".init"},     {Strtab, ".strtab"},          {Shstrtab, ".shstrtab"},         {Dynmaic, ".dynamic"},
        {Text, ".text"},        {TLS, ".tls"}};
};
