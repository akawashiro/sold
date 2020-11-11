#pragma once

#include <map>
#include <string>
#include <vector>

#include "hash.h"
#include "strtab_builder.h"
#include "utils.h"
#include "version_builder.h"

class SymtabBuilder {
public:
    SymtabBuilder();

    void SetSrcSyms(std::vector<Syminfo> syms);

    bool Resolve(const std::string& name, const std::string& filename, const std::string version_name, uintptr_t& val_or_index);

    uintptr_t ResolveCopy(const std::string& name, const std::string& filename, const std::string version_name);

    void Build(StrtabBuilder& strtab, VersionBuilder& version);

    void MergePublicSymbols(StrtabBuilder& strtab, VersionBuilder& version);

    void AddPublicSymbol(Syminfo s) {
        CHECK(is_special_ver_ndx(s.versym) || s.versym == VersionBuilder::NEED_NEW_VERNUM) << SOLD_LOG_KEY(s);
        public_syms_.push_back(s);
    }

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

    // map from (name, soname, version) to Versym and Sym
    std::map<std::tuple<std::string, std::string, std::string>, std::pair<Elf_Versym, Elf_Sym*> > src_syms_;
    std::map<std::tuple<std::string, std::string, std::string>, Symbol> syms_;

    std::vector<Syminfo> exposed_syms_;
    std::vector<Elf_Sym> symtab_;
    std::vector<Syminfo> public_syms_;

    Elf_GnuHash gnu_hash_;

    uintptr_t AddSym(const Syminfo& sym);
};
