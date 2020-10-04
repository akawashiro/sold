#pragma once

#include <elf.h>

#include <cassert>
#include <map>
#include <string>
#include <vector>

#include <glog/logging.h>

#define SOLD_LOG_KEY_VALUE(key, value) " " << LogBlue(key) << "=" << value
#define SOLD_LOG_KEY(key) SOLD_LOG_KEY_VALUE(#key, key)

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

const bool FLAGS_LOG{true};
extern bool QUIET_LOG;

#ifdef NOLOG
#define LOGF(...)                                               \
    do {                                                        \
        if (QUIET_LOG) break;                                   \
        if (0) fprintf(stderr, "%s, %d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                           \
    } while (0)
#else
#define LOGF(...)                                                       \
    do {                                                                \
        if (QUIET_LOG) break;                                           \
        if (FLAGS_LOG) fprintf(stderr, "%s, %d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                                   \
    } while (0)
#endif

template <class Stream>
inline void CatToStream(Stream& oss) {}

template <class Stream, class Arg0, class... Args>
inline void CatToStream(Stream& oss, Arg0 arg0, Args... args) {
    oss << arg0;
    CatToStream(oss, args...);
}

template <class... Args>
inline std::string StrCat(Args... args) {
    std::ostringstream oss;
    CatToStream(oss, args...);
    return oss.str();
}

const std::string& LogGreen();
const std::string& LogBlue();
const std::string& LogYellow();
const std::string& LogRed();
const std::string& LogReset();
std::string LogGreen(const std::string& msg);
std::string LogBlue(const std::string& msg);
std::string LogYellow(const std::string& msg);
std::string LogRed(const std::string& msg);

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

std::string special_ver_ndx_to_str(Elf64_Versym v);
