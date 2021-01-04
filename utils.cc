#include "utils.h"
#include <iomanip>

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

std::ostream& operator<<(std::ostream& os, const Syminfo& s) {
    auto f = os.flags();
    os << "Syminfo{name=" << s.name << ", soname=" << s.soname << ", version=" << s.version << ", versym=" << s.versym << ", sym=0x"
       << std::hex << std::setfill('0') << std::setw(16) << s.sym << "}";
    os.flags(f);
    return os;
}

std::string ShowDW_EH_PE(uint8_t type) {
    if (type == DW_EH_PE_omit) {
        return "DW_EH_PE_omit";
    }

    std::string ret;

    switch (type & 0xf) {
        case DW_EH_PE_absptr:
            ret = "DW_EH_PE_absptr";
            break;
        case DW_EH_PE_uleb128:
            ret = "DW_EH_PE_uleb128";
            break;
        case DW_EH_PE_udata2:
            ret = "DW_EH_PE_udata2";
            break;
        case DW_EH_PE_udata4:
            ret = "DW_EH_PE_udata4";
            break;
        case DW_EH_PE_udata8:
            ret = "DW_EH_PE_udata8";
            break;
        case DW_EH_PE_sleb128:
            ret = "DW_EH_PE_sleb128";
            break;
        case DW_EH_PE_sdata2:
            ret = "DW_EH_PE_sdata2";
            break;
        case DW_EH_PE_sdata4:
            ret = "DW_EH_PE_sdata4";
            break;
        case DW_EH_PE_sdata8:
            ret = "DW_EH_PE_sdata8";
            break;
    }

    switch (type & 0xf0) {
        case DW_EH_PE_pcrel:
            ret += " + DW_EH_PE_pcrel";
            break;
        case DW_EH_PE_textrel:
            ret += " + DW_EH_PE_textrel";
            break;
        case DW_EH_PE_datarel:
            ret += " + DW_EH_PE_datarel";
            break;
        case DW_EH_PE_funcrel:
            ret += " + DW_EH_PE_funcrel";
            break;
        case DW_EH_PE_aligned:
            ret += " + DW_EH_PE_aligned";
            break;
    }

    if (type == DW_EH_PE_SOLD_DUMMY) {
        ret = "DW_EH_PE_SOLD_DUMMY(0xEE)";
    } else if (ret == "") {
        ret = HexString(type, 2);
    }

    return ret;
}

std::string ShowRelocationType(int type) {
    switch (type) {
        case R_X86_64_NONE:
            return "R_X86_64_NONE";
        case R_X86_64_64:
            return "R_X86_64_64";
        case R_X86_64_PC32:
            return "R_X86_64_PC32";
        case R_X86_64_GOT32:
            return "R_X86_64_GOT32";
        case R_X86_64_PLT32:
            return "R_X86_64_PLT32";
        case R_X86_64_COPY:
            return "R_X86_64_COPY";
        case R_X86_64_GLOB_DAT:
            return "R_X86_64_GLOB_DAT";
        case R_X86_64_JUMP_SLOT:
            return "R_X86_64_JUMP_SLOT";
        case R_X86_64_RELATIVE:
            return "R_X86_64_RELATIVE";
        case R_X86_64_GOTPCREL:
            return "R_X86_64_GOTPCREL";
        case R_X86_64_32:
            return "R_X86_64_32";
        case R_X86_64_32S:
            return "R_X86_64_32S";
        case R_X86_64_16:
            return "R_X86_64_16";
        case R_X86_64_PC16:
            return "R_X86_64_PC16";
        case R_X86_64_8:
            return "R_X86_64_8";
        case R_X86_64_PC8:
            return "R_X86_64_PC8";
        case R_X86_64_DTPMOD64:
            return "R_X86_64_DTPMOD64";
        case R_X86_64_DTPOFF64:
            return "R_X86_64_DTPOFF64";
        case R_X86_64_TPOFF64:
            return "R_X86_64_TPOFF64";
        case R_X86_64_TLSGD:
            return "R_X86_64_TLSGD";
        case R_X86_64_TLSLD:
            return "R_X86_64_TLSLD";
        case R_X86_64_DTPOFF32:
            return "R_X86_64_DTPOFF32";
        case R_X86_64_GOTTPOFF:
            return "R_X86_64_GOTTPOFF";
        case R_X86_64_TPOFF32:
            return "R_X86_64_TPOFF32";
        case R_X86_64_PC64:
            return "R_X86_64_PC64";
        case R_X86_64_GOTOFF64:
            return "R_X86_64_GOTOFF64";
        case R_X86_64_GOTPC32:
            return "R_X86_64_GOTPC32";
        case R_X86_64_GOT64:
            return "R_X86_64_GOT64";
        case R_X86_64_GOTPCREL64:
            return "R_X86_64_GOTPCREL64";
        case R_X86_64_GOTPC64:
            return "R_X86_64_GOTPC64";
        case R_X86_64_GOTPLT64:
            return "R_X86_64_GOTPLT64";
        case R_X86_64_PLTOFF64:
            return "R_X86_64_PLTOFF64";
        case R_X86_64_SIZE32:
            return "R_X86_64_SIZE32";
        case R_X86_64_SIZE64:
            return "R_X86_64_SIZE64";
        case R_X86_64_GOTPC32_TLSDESC:
            return "R_X86_64_GOTPC32_TLSDESC";
        case R_X86_64_TLSDESC:
            return "R_X86_64_TLSDESC";
        case R_X86_64_IRELATIVE:
            return "R_X86_64_IRELATIVE";
        case R_X86_64_RELATIVE64:
            return "R_X86_64_RELATIVE64";
        case R_X86_64_GOTPCRELX:
            return "R_X86_64_GOTPCRELX";
        case R_X86_64_REX_GOTPCRELX:
            return "R_X86_64_REX_GOTPCRELX";
        case R_X86_64_NUM:
            return "R_X86_64_NUM";
        default: {
            return HexString(type, 4);
        }
    }
}

std::ostream& operator<<(std::ostream& os, const Elf_Rel& r) {
    os << "Elf_Rela{r_offset=" << SOLD_LOG_32BITS(r.r_offset) << ", r_info=" << SOLD_LOG_32BITS(r.r_info)
       << ", ELF_R_SYM(r.r_info)=" << SOLD_LOG_16BITS(ELF_R_SYM(r.r_info))
       << ", ELF_R_TYPE(r.r_info)=" << ShowRelocationType(ELF_R_TYPE(r.r_info)) << ", r_addend=" << SOLD_LOG_32BITS(r.r_addend) << "}";
    return os;
}

bool is_special_ver_ndx(Elf64_Versym versym) {
    return (versym == VER_NDX_LOCAL || versym == VER_NDX_GLOBAL);
}

std::string special_ver_ndx_to_str(Elf64_Versym versym) {
    if (versym == VER_NDX_LOCAL) {
        return std::string("VER_NDX_LOCAL");
    } else if (versym == VER_NDX_GLOBAL) {
        return std::string("VER_NDX_GLOBAL");
    } else if (versym == NO_VERSION_INFO) {
        return std::string("NO_VERSION_INFO");
    } else {
        LOG(FATAL) << "This versym (= " << versym << ") is not special.";
        exit(1);
    }
}
