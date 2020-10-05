#include "utils.h"

bool QUIET_LOG{false};

std::vector<std::string> SplitString(const std::string& str, const std::string& sep) {
    std::vector<std::string> ret;
    if (str.empty()) return ret;
    size_t index = 0;
    while (true) {
        size_t next = str.find(sep, index);
        ret.push_back(str.substr(index, next - index));
        if (next == std::string::npos) break;
        index = next + 1;
    }
    return ret;
}

bool HasPrefix(const std::string& str, const std::string& prefix) {
    ssize_t size_diff = str.size() - prefix.size();
    return size_diff >= 0 && str.substr(0, prefix.size()) == prefix;
}

uintptr_t AlignNext(uintptr_t a, uintptr_t mask) {
    return (a + mask) & ~mask;
}

bool IsTLS(const Elf_Sym& sym) {
    return ELF_ST_TYPE(sym.st_info) == STT_TLS;
}

bool IsDefined(const Elf_Sym& sym) {
    return (sym.st_value || IsTLS(sym)) && sym.st_shndx != SHN_UNDEF;
}

bool is_special_ver_ndx(Elf64_Versym versym) {
    return (versym == VER_NDX_LOCAL || versym == VER_NDX_GLOBAL);
}

std::string special_ver_ndx_to_str(Elf64_Versym versym) {
    if (versym == VER_NDX_LOCAL) {
        return std::string("VER_NDX_LOCAL");
    } else if (versym == VER_NDX_GLOBAL) {
        return std::string("VER_NDX_GLOBAL");
    } else {
        LOGF("This versym (= %d) is not special.\n", versym);
        exit(1);
    }
}
