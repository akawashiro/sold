#pragma once

#include <map>
#include <string>
#include <vector>

#include "hash.h"
#include "strtab_builder.h"
#include "utils.h"

class SymtabBuilder {
public:
    SymtabBuilder();

    void SetSrcSyms(std::vector<Syminfo> syms) { src_syms_ = syms; }

    bool Resolve(const std::string& name, uintptr_t& val_or_index);

    uintptr_t ResolveCopy(const std::string& name);

    void Build(StrtabBuilder& strtab);

    void MergePublicSymbols(StrtabBuilder& strtab);

    void AddPublicSymbol(const std::string& name, Elf_Sym sym) { public_syms_.emplace(name, sym); }

    uintptr_t size() const { return symtab_.size() + public_syms_.size(); }

    const Elf_GnuHash& gnu_hash() const { return gnu_hash_; }

    uintptr_t GnuHashSize() const;

    const std::vector<Elf_Sym>& Get() { return symtab_; }

    const std::vector<Syminfo>& GetExposedSyms() const { return exposed_syms_; }

private:
    struct Symbol {
        Elf_Sym sym;
        uintptr_t index;
    };

    std::vector<Syminfo> src_syms_;
    std::map<std::string, Symbol> syms_;

    std::vector<Syminfo> exposed_syms_;
    std::vector<Elf_Sym> symtab_;
    std::map<std::string, Elf_Sym> public_syms_;

    Elf_GnuHash gnu_hash_;

    uintptr_t AddSym(const Syminfo& sym);
};
