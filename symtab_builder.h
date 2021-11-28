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

    bool ResolveCopy(const std::string& name, const std::string& soname, const std::string version, uintptr_t* index);

    void Build(StrtabBuilder& strtab, VersionBuilder& version);

    void MergePublicSymbols(StrtabBuilder& strtab, VersionBuilder& version);

    void AddPublicSymbol(Syminfo s) { public_syms_.push_back(s); }

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

    // map from (name, soname, version) to (Versym, Sym*)
    std::map<std::tuple<std::string, std::string, std::string>, std::pair<Elf_Versym, Elf_Sym*> > src_syms_;
    // map from name to (Versym, Sym*)
    // We use src_fallback_syms_ when we don't have version information.
    std::map<std::string, std::pair<Elf_Versym, Elf_Sym*> > src_fallback_syms_;
    std::map<std::tuple<std::string, std::string, std::string>, Symbol> syms_;

    std::vector<Syminfo> exposed_syms_;
    std::vector<Elf_Sym> symtab_;
    std::vector<Syminfo> public_syms_;

    Elf_GnuHash gnu_hash_;

    uintptr_t AddSym(const Syminfo& sym);
};
