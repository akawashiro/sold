// Copyright (C) 2021 The sold authors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "hash.h"
#include "utils.h"

#include <cassert>
#include <limits>
#include <map>
#include <memory>
#include <set>

class ELFBinary {
public:
    ELFBinary(const std::string& filename, int fd, char* head, size_t mapped_size, size_t filesize);

    ~ELFBinary();

    static bool IsELF(const char* p);

    const std::string& filename() const { return filename_; }

    const Elf_Ehdr* ehdr() const { return ehdr_; }
    const std::vector<Elf_Phdr*> phdrs() const { return phdrs_; }
    const std::vector<Elf_Phdr*> loads() const { return loads_; }
    const Elf_Phdr* tls() const { return tls_; }
    const Elf_Phdr* gnu_stack() const { return gnu_stack_; }
    const Elf_Phdr* gnu_relro() const { return gnu_relro_; }

    const std::vector<std::string>& neededs() const { return neededs_; }
    const std::string& soname() const { return soname_; }
    const std::string& runpath() const { return runpath_; }
    const std::string& rpath() const { return rpath_; }

    Elf_Sym* symtab() const { return symtab_; }
    Elf_Verneed* verneed() const { return verneed_; }
    Elf_Versym* versym() const { return versym_; }
    const Elf_Xword verneednum() const { return verneednum_; }
    const Elf_Rel* rel() const { return rel_; }
    size_t num_rels() const { return num_rels_; }
    const Elf_Rel* plt_rel() const { return plt_rel_; }
    size_t num_plt_rels() const { return num_plt_rels_; }
    const EHFrameHeader* eh_frame_header() const { return &eh_frame_header_; }
    const char* strtab() const { return strtab_; }

    const char* head() const { return head_; }
    size_t filesize() const { return filesize_; }
    size_t mapped_size() const { return mapped_size_; }

    const std::string& name() const { return name_; }

    uintptr_t init() const { return init_; }
    uintptr_t fini() const { return fini_; }
    const uintptr_t init_array_addr() const { return init_array_addr_; };
    const uintptr_t fini_array_addr() const { return fini_array_addr_; };
    const std::vector<uintptr_t>& init_array() const { return init_array_; }
    const std::vector<uintptr_t>& fini_array() const { return fini_array_; }

    const std::vector<Syminfo>& GetSymbolMap() const { return syms_; }

    Range GetRange() const;

    bool IsAddrInInitarray(uintptr_t addr) const;
    bool IsAddrInFiniarray(uintptr_t addr) const;

    bool IsVaddrInTLSData(uintptr_t vaddr) const;
    bool IsOffsetInTLSData(uintptr_t offset) const;
    bool IsOffsetInTLSBSS(uintptr_t offset) const;

    std::set<int> CollectSymbolsFromDynamic();
    void ReadDynSymtab(const std::map<std::string, std::string>& filename_to_soname);

    const char* Str(uintptr_t name) { return strtab_ + name; }

    char* GetPtr(uintptr_t offset) { return head_ + OffsetFromAddr(offset); }

    Elf_Phdr* FindPhdr(uint64_t type);

    const Elf_Phdr& GetPhdr(uint64_t type);

    void PrintVerneeds();

    void PrintVersyms();

    std::string ShowDynSymtab();
    std::string ShowDtRela();
    std::string ShowVersion();
    std::string ShowTLS();
    std::string ShowEHFrame();

    std::pair<std::string, std::string> GetVersion(int index, const std::map<std::string, std::string>& filename_to_soname);

    Elf_Addr OffsetFromAddr(const Elf_Addr addr) const;
    Elf_Addr AddrFromOffset(const Elf_Addr offset) const;

private:
    void ParsePhdrs();
    void ParseEHFrameHeader(size_t off, size_t size);
    void ParseDynamic(size_t off, size_t size);
    void ParseFuncArray(uintptr_t* array, uintptr_t size, std::vector<uintptr_t>* out);

    const std::string filename_;
    int fd_;
    char* head_;
    size_t filesize_;
    size_t mapped_size_;

    Elf_Ehdr* ehdr_{nullptr};
    std::vector<Elf_Phdr*> phdrs_;
    std::vector<Elf_Phdr*> loads_;
    Elf_Phdr* tls_{nullptr};
    Elf_Phdr* gnu_stack_{nullptr};
    Elf_Phdr* gnu_relro_{nullptr};
    const char* strtab_{nullptr};
    Elf_Sym* symtab_{nullptr};

    EHFrameHeader eh_frame_header_;

    std::vector<std::string> neededs_;
    // This is the name specified in the DT_SONAME field.
    std::string soname_;
    std::string runpath_;
    std::string rpath_;

    Elf_Rel* rel_{nullptr};
    size_t num_rels_{0};
    Elf_Rel* plt_rel_{nullptr};
    size_t num_plt_rels_{0};

    Elf_GnuHash* gnu_hash_{nullptr};
    Elf_Hash* hash_{nullptr};

    uintptr_t* init_array_offset_{0};
    uintptr_t init_arraysz_{0};
    uintptr_t init_array_addr_{0};
    uintptr_t* fini_array_offset_{0};
    uintptr_t fini_array_addr_{0};
    uintptr_t fini_arraysz_{0};

    uintptr_t init_{0};
    uintptr_t fini_{0};
    std::vector<uintptr_t> init_array_;
    std::vector<uintptr_t> fini_array_;

    // This is the name on the filsysytem
    std::string name_;
    std::vector<Syminfo> syms_;

    int nsyms_{0};

    Elf_Versym* versym_{nullptr};
    Elf_Verneed* verneed_{nullptr};
    Elf_Verdef* verdef_{nullptr};
    Elf_Xword verneednum_{0};
    Elf_Xword verdefnum_{0};
};

std::unique_ptr<ELFBinary> ReadELF(const std::string& filename);
