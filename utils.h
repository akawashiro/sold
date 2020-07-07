#pragma once

#include <elf.h>

#include <string>
#include <vector>

#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Dyn Elf64_Dyn
#define Elf_Addr Elf64_Addr
#define Elf_Sym Elf64_Sym
#define Elf_Versym Elf64_Versym
#define Elf_Word Elf64_Xword
#define Elf_Vernaux Elf64_Vernaux
#define Elf_Verneed Elf64_Verneed
#define ELF_ST_BIND(val) ELF64_ST_BIND(val)
#define ELF_ST_TYPE(val) ELF64_ST_TYPE(val)
#define ELF_ST_INFO(bind, type) ELF64_ST_INFO(bind, type)
#define Elf_Rel Elf64_Rela
#define ELF_R_SYM(val) ELF64_R_SYM(val)
#define ELF_R_TYPE(val) ELF64_R_TYPE(val)
#define ELF_R_INFO(sym, type) ELF64_R_INFO(sym, type)

#define CHECK(r)             \
    do {                     \
        if (!(r)) assert(r); \
    } while (0)

const bool FLAGS_LOG{true};

#ifdef NOLOG
#define LOGF(...)                                          \
    if (0) fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__)
#else
#define LOGF(...)                                                  \
    if (FLAGS_LOG) fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__)
#endif

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
