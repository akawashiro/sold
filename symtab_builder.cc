#include "symtab_builder.h"

#include <limits>

SymtabBuilder::SymtabBuilder() {
    Syminfo si;
    si.name = "";
    si.filename = "";
    si.version_name = "";
    si.ver = 0;
    si.sym = NULL;

    Symbol sym{};

    AddSym(si);
    CHECK(syms_.emplace("", sym).second);
}

uintptr_t SymtabBuilder::AddSym(const Syminfo& sym) {
    uintptr_t index = exposed_syms_.size();
    exposed_syms_.push_back(sym);
    return index;
}

bool SymtabBuilder::Resolve(const std::string& name, uintptr_t& val_or_index) {
    Symbol sym{};
    sym.sym.st_name = 0;
    sym.sym.st_info = 0;
    sym.sym.st_other = 0;
    sym.sym.st_shndx = 0;
    sym.sym.st_value = 0;
    sym.sym.st_size = 0;

    auto found = syms_.find(name);
    if (found != syms_.end()) {
        sym = found->second;
    } else {
        Syminfo* found = NULL;
        for (int i = 0; i < src_syms_.size(); i++) {
            if (src_syms_[i].name == name) {
                found = &src_syms_[i];
            }
        }

        if (found != NULL) {
            sym.sym = *found->sym;
            if (IsDefined(sym.sym)) {
                LOGF("Symbol %s found\n", name.c_str());
            } else {
                LOGF("Symbol (undef/weak) %s found\n", name.c_str());
                Syminfo s{name, "", "", 0, NULL};
                sym.index = AddSym(s);
                CHECK(syms_.emplace(name, sym).second);
            }
        } else {
            LOGF("Symbol %s not found\n", name.c_str());
            Syminfo s{name, "", "", 0, NULL};
            sym.index = AddSym(s);
            CHECK(syms_.emplace(name, sym).second);
        }
    }

    if (!sym.sym.st_value) {
        val_or_index = sym.index;
        return false;
    } else {
        val_or_index = sym.sym.st_value;
        return true;
    }
}

uintptr_t SymtabBuilder::ResolveCopy(const std::string& name) {
    // TODO(hamaji): Refactor.
    Symbol sym{};
    sym.sym.st_name = 0;
    sym.sym.st_info = 0;
    sym.sym.st_other = 0;
    sym.sym.st_shndx = 0;
    sym.sym.st_value = 0;
    sym.sym.st_size = 0;

    auto found = syms_.find(name);
    if (found != syms_.end()) {
        sym = found->second;
    } else {
        Syminfo* found = NULL;
        for (int i = 0; i < src_syms_.size(); i++) {
            if (src_syms_[i].name == name) {
                found = &src_syms_[i];
            }
        }

        if (found != NULL) {
            LOGF("Symbol %s found for copy\n", name.c_str());
            sym.sym = *found->sym;
            Syminfo s{name, "", "", 0, NULL};
            sym.index = AddSym(s);
            CHECK(syms_.emplace(name, sym).second);
        } else {
            LOGF("Symbol %s not found for copy\n", name.c_str());
            CHECK(false);
        }
    }

    return sym.index;
}

void SymtabBuilder::Build(StrtabBuilder& strtab) {
    for (const auto& s : exposed_syms_) {
        auto found = syms_.find(s.name);
        CHECK(found != syms_.end());
        Elf_Sym sym = found->second.sym;
        sym.st_name = strtab.Add(s.name);
        symtab_.push_back(sym);
    }
}

void SymtabBuilder::MergePublicSymbols(StrtabBuilder& strtab) {
    gnu_hash_.nbuckets = 1;
    CHECK(symtab_.size() <= std::numeric_limits<uint32_t>::max());
    gnu_hash_.symndx = symtab_.size();
    gnu_hash_.maskwords = 1;
    gnu_hash_.shift2 = 1;

    for (const auto& p : public_syms_) {
        const std::string& name = p.name;
        Elf_Sym sym = *p.sym;
        sym.st_name = strtab.Add(name);
        sym.st_shndx = 1;

        Syminfo s{name, "", "", 0, NULL};
        exposed_syms_.push_back(s);
        symtab_.push_back(sym);
    }
    public_syms_.clear();
}

uintptr_t SymtabBuilder::GnuHashSize() const {
    CHECK(!symtab_.empty());
    CHECK(public_syms_.empty());
    CHECK(gnu_hash_.nbuckets);
    return (sizeof(uint32_t) * 4 + sizeof(Elf_Addr) + sizeof(uint32_t) * (1 + symtab_.size() - gnu_hash_.symndx));
}
