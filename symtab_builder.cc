#include "symtab_builder.h"
#include "version_builder.h"

#include <limits>

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

void SymtabBuilder::SetSrcSyms(std::vector<Syminfo> syms) {
    for (const auto& s : syms) {
        auto p = src_syms_.find({s.name, s.soname, s.version});
        // TODO(akirakawata) Do we need this if? LoadDynSymtab should returns
        // unique symbols therefore p == src_syms_.end() is true always.
        if (p == src_syms_.end() || p->second.second == NULL || !IsDefined(*p->second.second)) {
            src_syms_[{s.name, s.soname, s.version}] = {s.versym, s.sym};
        }
    }
}

uintptr_t SymtabBuilder::AddSym(const Syminfo& sym) {
    uintptr_t index = exposed_syms_.size();
    exposed_syms_.push_back(sym);
    return index;
}

// Returns true and fills st_value to value_or_index when the symbol specified
// with (name, soname, version) is defined.
// When the specified symbol is not defined, SymtabBuilder::Resolve pushes it
// to sym_ and exposed_syms_ and fills the index of the added symbol to
// val_or_index.
// TODO(akawashiro) Rename syms_.
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
        auto found = src_syms_.find({name, soname, version});

        if (found != src_syms_.end()) {
            sym.sym = *found->second.second;
            if (IsDefined(sym.sym)) {
                LOG(INFO) << "Symbol (" << name << ", " << soname << ", " << version << ") found";
                // TODO(akawashiro) TABUN KOKO
                Syminfo s{name, soname, version, found->second.first, NULL};
                sym.index = AddSym(s);
                CHECK(syms_.emplace(std::make_tuple(name, soname, version), sym).second);
            } else {
                LOG(INFO) << "Symbol (undef/weak) (" << name << ", " << soname << ", " << version << ") found";
                Syminfo s{name, soname, version, found->second.first, NULL};
                sym.index = AddSym(s);
                CHECK(syms_.emplace(std::make_tuple(name, soname, version), sym).second);
            }
        } else {
            LOG(INFO) << "Symbol (" << name << ", " << soname << ", " << version << ") not found";
            Syminfo s{name, soname, version, VER_NDX_LOCAL, NULL};
            sym.index = AddSym(s);
            CHECK(syms_.emplace(std::make_tuple(name, soname, version), sym).second);
        }
    }

    if (!IsDefined(sym.sym)) {
        val_or_index = sym.index;
        return false;
    } else {
        val_or_index = sym.sym.st_value;
        return true;
    }
}

// Returns the index of symbol(name, soname, version)
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
        auto found = src_syms_.find({name, soname, version});

        if (found != src_syms_.end()) {
            LOG(INFO) << "Symbol " << name << " found for copy";
            sym.sym = *found->second.second;
            Syminfo s{name, soname, version, found->second.first, NULL};
            sym.index = AddSym(s);
            CHECK(syms_.emplace(std::make_tuple(name, soname, version), sym).second);
        } else {
            LOG(INFO) << "Symbol " << name << " not found for copy";
            CHECK(false);
        }
    }

    return sym.index;
}

// Make a new symbol table(symtab_) from exposed_syms_.
void SymtabBuilder::Build(StrtabBuilder& strtab, VersionBuilder& version) {
    for (const Syminfo& s : exposed_syms_) {
        LOG(INFO) << "SymtabBuilder::Build " << SOLD_LOG_KEY(s);

        auto found = syms_.find({s.name, s.soname, s.version});
        CHECK(found != syms_.end());
        Elf_Sym sym = found->second.sym;
        sym.st_name = strtab.Add(s.name);
        // TODO(akawashiro)
        // I fill st_shndx with a dummy value which is not special section index.
        // After I make complete section headers, I should fill it with the right section index.
        if (sym.st_shndx != SHN_UNDEF && sym.st_shndx < SHN_LORESERVE) sym.st_shndx = 1;
        symtab_.push_back(sym);

        version.Add(s.versym, s.soname, s.version, strtab, sym.st_info);
    }
}

// Pushes all public_syms_ into exposed_syms_ and symtab_.
// TODO(akawashiro) Do we need changing exposed_syms_ here?
void SymtabBuilder::MergePublicSymbols(StrtabBuilder& strtab, VersionBuilder& version) {
    gnu_hash_.nbuckets = 1;
    CHECK(symtab_.size() <= std::numeric_limits<uint32_t>::max());
    gnu_hash_.symndx = symtab_.size();
    gnu_hash_.maskwords = 1;
    gnu_hash_.shift2 = 1;

    for (const auto& p : public_syms_) {
        LOG(INFO) << "SymtabBuilder::MergePublicSymbols " << p.name;

        const std::string& name = p.name;
        Elf_Sym* sym = new Elf_Sym;
        *sym = *p.sym;
        sym->st_name = strtab.Add(name);
        // TODO(akawashiro)
        // I fill st_shndx with a dummy value which is not special section index.
        // After I make complete section headers, I should fill it with the right section index.
        sym->st_shndx = 1;

        Syminfo s{p.name, p.soname, p.version, p.versym, sym};
        exposed_syms_.push_back(s);
        symtab_.push_back(*sym);

        version.Add(s.versym, s.soname, s.version, strtab, sym->st_info);
    }
    public_syms_.clear();
}

uintptr_t SymtabBuilder::GnuHashSize() const {
    CHECK(!symtab_.empty());
    CHECK(public_syms_.empty());
    CHECK(gnu_hash_.nbuckets);
    return (sizeof(uint32_t) * 4 + sizeof(Elf_Addr) + sizeof(uint32_t) * (1 + symtab_.size() - gnu_hash_.symndx));
}
