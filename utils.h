#pragma once

#include <elf.h>
#include <libdwarf/dwarf.h>

#include <cassert>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <glog/logging.h>

#define SOLD_LOG_KEY_VALUE(key, value) " " << key << "=" << value
#define SOLD_LOG_KEY(key) SOLD_LOG_KEY_VALUE(#key, key)
#define SOLD_LOG_64BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 16))
#define SOLD_LOG_32BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 8))
#define SOLD_LOG_16BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 4))
#define SOLD_LOG_8BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 2))
#define SOLD_LOG_DWEHPE(type) SOLD_LOG_KEY_VALUE(#type, ShowDW_EH_PE(type))

#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Dyn Elf64_Dyn
#define Elf_Addr Elf64_Addr
#define Elf_Sym Elf64_Sym
#define Elf_Shdr Elf64_Shdr
#define Elf_Off Elf64_Off
#define Elf_Half Elf64_Half
#define Elf_Versym Elf64_Versym
#define Elf_Xword Elf64_Xword
#define Elf_Word Elf64_Word
#define Elf_Vernaux Elf64_Vernaux
#define Elf_Verdaux Elf64_Verdaux
#define Elf_Verneed Elf64_Verneed
#define Elf_Verdef Elf64_Verdef
#define ELF_ST_BIND(val) ELF64_ST_BIND(val)
#define ELF_ST_TYPE(val) ELF64_ST_TYPE(val)
#define ELF_ST_INFO(bind, type) ELF64_ST_INFO(bind, type)
#define Elf_Rel Elf64_Rela
#define ELF_R_SYM(val) ELF64_R_SYM(val)
#define ELF_R_TYPE(val) ELF64_R_TYPE(val)
#define ELF_R_INFO(sym, type) ELF64_R_INFO(sym, type)

// Although there is no description of VERSYM_HIDDEN in glibc, you can find it
// in binutils source code.
// https://github.com/gittup/binutils/blob/8db2e9c8d085222ac7b57272ee263733ae193565/include/elf/common.h#L816
#define VERSYM_HIDDEN 0x8000
#define VERSYM_VERSION 0x7fff

static const Elf_Versym NO_VERSION_INFO = 0xffff;

std::vector<std::string> SplitString(const std::string& str, const std::string& sep);

bool HasPrefix(const std::string& str, const std::string& prefix);

uintptr_t AlignNext(uintptr_t a, uintptr_t mask = 4095);

struct Range {
    uintptr_t start;
    uintptr_t end;
    ptrdiff_t size() const { return end - start; }
    Range operator+(uintptr_t offset) const { return Range{start + offset, end + offset}; }
};

bool IsTLS(const Elf_Sym& sym);

bool IsDefined(const Elf_Sym& sym);

class ELFBinary;

struct Syminfo {
    std::string name;
    std::string soname;
    std::string version;
    Elf_Versym versym;
    Elf_Sym* sym;
};

std::string ShowRelocationType(int type);
std::string ShowDW_EH_PE(uint8_t type);
std::ostream& operator<<(std::ostream& os, const Syminfo& s);
std::ostream& operator<<(std::ostream& os, const Elf_Rel& s);

struct TLS {
    struct Data {
        ELFBinary* bin;
        uint8_t* start;
        size_t size;
        uintptr_t file_offset;
        uintptr_t bss_offset;
    };

    std::vector<Data> data;
    std::map<ELFBinary*, size_t> bin_to_index;
    uintptr_t filesz{0};
    uintptr_t memsz{0};
    Elf_Xword align{0};
};

bool is_special_ver_ndx(Elf64_Versym v);

std::string special_ver_ndx_to_str(Elf_Versym v);

template <class T>
inline std::string HexString(T num, int length = 16) {
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::setfill('0') << std::setw(length) << std::hex << +num;
    return ss.str();
}

static char* read_uleb128(char* p, uint32_t* val) {
    unsigned int shift = 0;
    unsigned char byte;
    uint32_t result;

    result = 0;
    do {
        byte = *p++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    *val = result;
    return p;
}

static char* read_sleb128(char* p, int32_t* val) {
    unsigned int shift = 0;
    unsigned char byte;
    int32_t result;

    result = 0;
    do {
        byte = *p++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    /* Sign-extend a negative value.  */
    if (shift < 8 * sizeof(result) && (byte & 0x40) != 0) result |= -(1L << shift);

    *val = (int32_t)result;
    return p;
}

typedef unsigned _Unwind_Ptr __attribute__((__mode__(__word__)));
typedef unsigned _Unwind_Internal_Ptr __attribute__((__mode__(__pointer__)));
typedef unsigned _Unwind_Word __attribute__((__mode__(__unwind_word__)));
typedef signed _Unwind_Sword __attribute__((__mode__(__unwind_word__)));

#define DW_EH_PE_indirect 0x80

static char* read_encoded_value_with_base(unsigned char encoding, _Unwind_Ptr base, char* p, uint32_t* val) {
    union unaligned {
        void* ptr;
        unsigned u2 __attribute__((mode(HI)));
        unsigned u4 __attribute__((mode(SI)));
        unsigned u8 __attribute__((mode(DI)));
        signed s2 __attribute__((mode(HI)));
        signed s4 __attribute__((mode(SI)));
        signed s8 __attribute__((mode(DI)));
    } __attribute__((__packed__));

    union unaligned* u = (union unaligned*)p;
    _Unwind_Internal_Ptr result;

    if (encoding == DW_EH_PE_aligned) {
        _Unwind_Internal_Ptr a = (_Unwind_Internal_Ptr)p;
        a = (a + sizeof(void*) - 1) & -sizeof(void*);
        result = *(_Unwind_Internal_Ptr*)a;
        p = (char*)(a + sizeof(void*));
    } else {
        switch (encoding & 0x0f) {
            case DW_EH_PE_absptr:
                result = (_Unwind_Internal_Ptr)u->ptr;
                p += sizeof(void*);
                break;

            case DW_EH_PE_uleb128: {
                uint32_t tmp;
                p = read_uleb128(p, &tmp);
                result = (_Unwind_Internal_Ptr)tmp;
            } break;

            case DW_EH_PE_sleb128: {
                int32_t tmp;
                p = read_sleb128(p, &tmp);
                result = (_Unwind_Internal_Ptr)tmp;
            } break;

            case DW_EH_PE_udata2:
                result = u->u2;
                p += 2;
                break;
            case DW_EH_PE_udata4:
                result = u->u4;
                p += 4;
                break;
            case DW_EH_PE_udata8:
                result = u->u8;
                p += 8;
                break;

            case DW_EH_PE_sdata2:
                result = u->s2;
                p += 2;
                break;
            case DW_EH_PE_sdata4:
                result = u->s4;
                p += 4;
                break;
            case DW_EH_PE_sdata8:
                result = u->s8;
                p += 8;
                break;
            default:
                LOG(FATAL) << SOLD_LOG_8BITS(encoding & 0x0f);
                // __gxx_abort();
        }

        if (result != 0) {
            result += ((encoding & 0x70) == DW_EH_PE_pcrel ? (_Unwind_Internal_Ptr)u : base);
            if (encoding & DW_EH_PE_indirect) result = *(_Unwind_Internal_Ptr*)result;
        }
    }

    *val = result;
    return p;
}
