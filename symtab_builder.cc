#include "symtab_builder.h"
#include "version_builder.h"

#include <limits>

namespace {
Elf_Versym newver(Elf_Versym ver) {
    if (ver == VER_NDX_LOCAL || ver == VER_NDX_GLOBAL) {
        return ver;
    } else {
        return VersionBuilder::NEED_NEW_VERNUM;
    }
}
}  // namespace

SymtabBuilder::SymtabBuilder() {
    Syminfo si;
    si.name = "";
    si.soname = "";
    si.version = "";
    si.versym = VER_NDX_LOCAL;
    si.sym = NULL;

    Symbol sym{};

    AddSym(si);
    CHECK(syms_.emplace(std::make_tuple("", "", ""), sym).second);
}

uintptr_t SymtabBuilder::AddSym(const Syminfo& sym) {
    uintptr_t index = exposed_syms_.size();
    exposed_syms_.push_back(sym);
    return index;
}

bool SymtabBuilder::Resolve(const std::string& name, const std::string& soname, const std::string version, uintptr_t& val_or_index) {
    Symbol sym{};
    sym.sym.st_name = 0;
    sym.sym.st_info = 0;
    sym.sym.st_other = 0;
    sym.sym.st_shndx = 0;
    sym.sym.st_value = 0;
    sym.sym.st_size = 0;

    auto found = syms_.find({name, soname, version});
    if (found != syms_.end()) {
        sym = found->second;
    } else {
        Syminfo* found = NULL;
        for (int i = 0; i < src_syms_.size(); i++) {
            if (src_syms_[i].name == name && src_syms_[i].soname == soname && src_syms_[i].version == version) {
                found = &src_syms_[i];
            }
        }

        if (found != NULL) {
            sym.sym = *found->sym;
            if (IsDefined(sym.sym)) {
                LOGF("Symbol %s found\n", name.c_str());
            } else {
                LOGF("Symbol (undef/weak) %s(soname = %s, sym.sym.st_value = %d) found\n", name.c_str(), soname.c_str(), sym.sym.st_value);
                Syminfo s{name, soname, version, newver(found->versym), NULL};
                sym.index = AddSym(s);
                CHECK(syms_.emplace(std::make_tuple(name, soname, version), sym).second);
            }
        } else {
            LOGF("Symbol %s not found\n", name.c_str());
            Syminfo s{name, soname, version, VER_NDX_LOCAL, NULL};
            sym.index = AddSym(s);
            CHECK(syms_.emplace(std::make_tuple(name, soname, version), sym).second);
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

uintptr_t SymtabBuilder::ResolveCopy(const std::string& name, const std::string& soname, const std::string version) {
    // TODO(hamaji): Refactor.
    Symbol sym{};
    sym.sym.st_name = 0;
    sym.sym.st_info = 0;
    sym.sym.st_other = 0;
    sym.sym.st_shndx = 0;
    sym.sym.st_value = 0;
    sym.sym.st_size = 0;

    auto found = syms_.find({name, soname, version});
    if (found != syms_.end()) {
        sym = found->second;
    } else {
        Syminfo* found = NULL;
        for (int i = 0; i < src_syms_.size(); i++) {
            if (src_syms_[i].name == name && src_syms_[i].soname == soname && src_syms_[i].version == version) {
                found = &src_syms_[i];
            }
        }

        if (found != NULL) {
            LOGF("Symbol %s found for copy\n", name.c_str());
            sym.sym = *found->sym;
            Syminfo s{name, soname, version, newver(found->versym), NULL};
            sym.index = AddSym(s);
            CHECK(syms_.emplace(std::make_tuple(name, soname, version), sym).second);
        } else {
            LOGF("Symbol %s not found for copy\n", name.c_str());
            CHECK(false);
        }
    }

    return sym.index;
}

void SymtabBuilder::Build(StrtabBuilder& strtab, VersionBuilder& version) {
    for (const auto& s : exposed_syms_) {
        LOGF("SymtabBuilder::Build %s\n", s.name.c_str());

        auto found = syms_.find({s.name, s.soname, s.version});
        CHECK(found != syms_.end());
        Elf_Sym sym = found->second.sym;
        sym.st_name = strtab.Add(s.name);
        // TODO(akawashiro)
        // I fill st_shndx with a dummy value which is not special section index.
        // After I make complete section headers, I should fill it with the right section index.
        if (sym.st_shndx != SHN_UNDEF && sym.st_shndx < SHN_LORESERVE) sym.st_shndx = 1;
        symtab_.push_back(sym);

        version.Add(s.versym, s.soname, s.version, strtab);
    }
}

void SymtabBuilder::MergePublicSymbols(StrtabBuilder& strtab, VersionBuilder& version) {
    gnu_hash_.nbuckets = 1;
    CHECK(symtab_.size() <= std::numeric_limits<uint32_t>::max());
    gnu_hash_.symndx = symtab_.size();
    gnu_hash_.maskwords = 1;
    gnu_hash_.shift2 = 1;

    for (const auto& p : public_syms_) {
        LOGF("SymtabBuilder::MergePublicSymbols %s\n", p.name.c_str());

        const std::string& name = p.name;
        Elf_Sym* sym = new Elf_Sym;
        *sym = *p.sym;
        sym->st_name = strtab.Add(name);
        // TODO(akawashiro)
        // I fill st_shndx with a dummy value which is not special section index.
        // After I make complete section headers, I should fill it with the right section index.
        sym->st_shndx = 1;

        Syminfo s{p.name, p.soname, p.version, VER_NDX_GLOBAL, sym};
        exposed_syms_.push_back(s);
        symtab_.push_back(*sym);

        version.Add(s.versym, s.soname, s.version, strtab);
    }
    public_syms_.clear();
}

uintptr_t SymtabBuilder::GnuHashSize() const {
    CHECK(!symtab_.empty());
    CHECK(public_syms_.empty());
    CHECK(gnu_hash_.nbuckets);
    return (sizeof(uint32_t) * 4 + sizeof(Elf_Addr) + sizeof(uint32_t) * (1 + symtab_.size() - gnu_hash_.symndx));
}
