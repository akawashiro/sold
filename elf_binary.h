#pragma once

#include "hash.h"
#include "utils.h"

#include <cassert>
#include <limits>
#include <map>
#include <memory>

class ELFBinary {
public:
    ELFBinary(const std::string& filename, int fd, char* head, size_t size);

    ~ELFBinary();

    static bool IsELF(const char* p);

    const std::string& filename() const { return filename_; }

    const Elf_Ehdr* ehdr() const { return ehdr_; }
    const std::vector<Elf_Phdr*> phdrs() const { return phdrs_; }
    const std::vector<Elf_Phdr*> loads() const { return loads_; }
    const Elf_Phdr* tls() const { return tls_; }

    const std::vector<std::string>& neededs() const { return neededs_; }
    const std::string& soname() const { return soname_; }
    const std::string& runpath() const { return runpath_; }
    const std::string& rpath() const { return rpath_; }

    const Elf_Sym* symtab() const { return symtab_; }
    const Elf_Rel* rel() const { return rel_; }
    size_t num_rels() const { return num_rels_; }
    const Elf_Rel* plt_rel() const { return plt_rel_; }
    size_t num_plt_rels() const { return num_plt_rels_; }

    const char* head() const { return head_; }

    const std::string& name() const { return name_; }

    uintptr_t init() const { return init_; }
    uintptr_t fini() const { return fini_; }
    const std::vector<uintptr_t>& init_array() const { return init_array_; }
    const std::vector<uintptr_t>& fini_array() const { return fini_array_; }

    const std::vector<Syminfo>& GetSymbolMap() const { return syms_; }

    Range GetRange() const;

    bool InTLS(uintptr_t offset) const;

    void ReadDynSymtab();

    const char* Str(uintptr_t name) { return strtab_ + name; }

    char* GetPtr(uintptr_t offset) { return head_ + OffsetFromAddr(offset); }

    Elf_Phdr* FindPhdr(uint64_t type);

    const Elf_Phdr& GetPhdr(uint64_t type);

    void PrintVerneeds();

    void PrintVersyms();

    std::string ShowDynSymtab();

    std::string ShowDtRela();

    std::string ShowVersym(int index);

    std::pair<std::string, std::string> GetVerneed(int index);

private:
    void ParsePhdrs();

    void ParseDynamic(size_t off, size_t size);

    void ParseFuncArray(uintptr_t* array, uintptr_t size, std::vector<uintptr_t>* out);

    Elf_Addr OffsetFromAddr(Elf_Addr addr);

    const std::string filename_;
    int fd_;
    char* head_;
    size_t size_;

    Elf_Ehdr* ehdr_{nullptr};
    std::vector<Elf_Phdr*> phdrs_;
    std::vector<Elf_Phdr*> loads_;
    Elf_Phdr* tls_{nullptr};
    const char* strtab_{nullptr};
    Elf_Sym* symtab_{nullptr};

    std::vector<std::string> neededs_;
    std::string soname_;
    std::string runpath_;
    std::string rpath_;

    Elf_Rel* rel_{nullptr};
    size_t num_rels_{0};
    Elf_Rel* plt_rel_{nullptr};
    size_t num_plt_rels_{0};

    Elf_GnuHash* gnu_hash_{nullptr};
    Elf_Hash* hash_{nullptr};

    uintptr_t init_{0};
    uintptr_t fini_{0};
    std::vector<uintptr_t> init_array_;
    std::vector<uintptr_t> fini_array_;

    std::string name_;
    std::vector<Syminfo> syms_;

    int nsyms_{0};

    Elf_Versym* versym_{nullptr};
    Elf_Verneed* verneed_{nullptr};
    Elf_Word verneednum_{0};
};

std::unique_ptr<ELFBinary> ReadELF(const std::string& filename);
