#include <assert.h>
#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#include "elf_binary.h"
#include "hash.h"
#include "ldsoconf.h"
#include "utils.h"

class StrtabBuilder {
public:
    uintptr_t Add(const std::string& s) {
        CHECK(!is_freezed_);
        uintptr_t pos = static_cast<uintptr_t>(strtab_.size());
        strtab_ += s;
        strtab_ += '\0';
        return pos;
    }

    void Freeze() { is_freezed_ = true; }

    size_t size() const { return strtab_.size(); }

    const void* data() const { return strtab_.data(); }

private:
    std::string strtab_;
    bool is_freezed_{false};
};

class SymtabBuilder {
public:
    struct Symbol {
        Elf_Sym sym;
        uintptr_t index;
    };

    void SetSrcSyms(std::map<std::string, Elf_Sym*> syms) { src_syms_ = syms; }

    uintptr_t AddSym(const std::string& name) {
        uintptr_t index = sym_names_.size();
        sym_names_.push_back(name);
        return index;
    }

    bool Resolve(const std::string& name, uintptr_t* val_or_index) {
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
            auto found = src_syms_.find(name);
            if (found != src_syms_.end()) {
                sym.sym = *found->second;
                if (IsDefined(sym.sym)) {
                    LOGF("Symbol %s found\n", name.c_str());
                } else {
                    LOGF("Symbol (undef/weak) %s found\n", name.c_str());
                    sym.index = AddSym(name);
                    CHECK(syms_.emplace(name, sym).second);
                }
            } else {
                LOGF("Symbol %s not found\n", name.c_str());
                sym.index = AddSym(name);
                CHECK(syms_.emplace(name, sym).second);
            }
        }

        if (!sym.sym.st_value) {
            *val_or_index = sym.index;
            return false;
        } else {
            *val_or_index = sym.sym.st_value;
            return true;
        }
    }

    uintptr_t ResolveCopy(const std::string& name) {
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
            auto found = src_syms_.find(name);
            if (found != src_syms_.end()) {
                LOGF("Symbol %s found for copy\n", name.c_str());
                sym.sym = *found->second;
                sym.index = AddSym(name);
                CHECK(syms_.emplace(name, sym).second);
            } else {
                LOGF("Symbol %s not found for copy\n", name.c_str());
                CHECK(false);
            }
        }

        return sym.index;
    }

    void Build(StrtabBuilder* strtab) {
        for (const std::string& name : sym_names_) {
            auto found = syms_.find(name);
            CHECK(found != syms_.end());
            Elf_Sym sym = found->second.sym;
            sym.st_name = strtab->Add(name);
            symtab_.push_back(sym);
        }
    }

    void AddPublicSymbol(const std::string& name, Elf_Sym sym) { public_syms_.emplace(name, sym); }

    void MergePublicSymbols(StrtabBuilder* strtab) {
        gnu_hash_.nbuckets = 1;
        CHECK(symtab_.size() <= std::numeric_limits<uint32_t>::max());
        gnu_hash_.symndx = symtab_.size();
        gnu_hash_.maskwords = 1;
        gnu_hash_.shift2 = 1;

        for (const auto& p : public_syms_) {
            const std::string& name = p.first;
            Elf_Sym sym = p.second;
            sym.st_name = strtab->Add(name);
            sym.st_shndx = 1;
            sym_names_.push_back(name);
            symtab_.push_back(sym);
        }
        public_syms_.clear();
    }

    uintptr_t size() const { return symtab_.size() + public_syms_.size(); }

    const Elf_GnuHash& gnu_hash() const { return gnu_hash_; }

    uintptr_t gnu_hash_size() const {
        CHECK(!symtab_.empty());
        CHECK(public_syms_.empty());
        CHECK(gnu_hash_.nbuckets);
        return (sizeof(uint32_t) * 4 + sizeof(Elf_Addr) + sizeof(uint32_t) * (1 + symtab_.size() - gnu_hash_.symndx));
    }

    const std::vector<Elf_Sym>& Get() { return symtab_; }

    const std::vector<std::string>& GetNames() const { return sym_names_; }

private:
    std::map<std::string, Elf_Sym*> src_syms_;
    std::map<std::string, Symbol> syms_;

    std::vector<std::string> sym_names_;
    std::vector<Elf_Sym> symtab_;
    std::map<std::string, Elf_Sym> public_syms_;

    Elf_GnuHash gnu_hash_;
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
};

class Sold {
public:
    Sold(const std::string& elf_filename) {
        main_binary_ = ReadELF(elf_filename);
        is_executable_ = main_binary_->FindPhdr(PT_INTERP);
        link_binaries_.push_back(main_binary_.get());

        InitLdLibraryPaths();
        ResolveLibraryPaths(main_binary_.get());
    }

    void Link(const std::string& out_filename) {
        DecideOffsets();
        CollectTLS();
        CollectArrays();
        CollectSymbols();
        PrintAllVerneeds();
        PrintAllVersyms();
        CopyPublicSymbols();
        Relocate();

        syms_.Build(&strtab_);
        syms_.MergePublicSymbols(&strtab_);

        BuildEhdr();
        if (is_executable_) {
            BuildInterp();
        }
        BuildArrays();
        BuildDynamic();

        strtab_.Freeze();
        BuildLoads();

        Emit(out_filename);
        CHECK(chmod(out_filename.c_str(), 0755) == 0);
    }

private:
    template <class T>
    void Write(FILE* fp, const T& v) {
        CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);
    }

    void WriteBuf(FILE* fp, const void* buf, size_t size) { CHECK(fwrite(buf, 1, size, fp) == size); }

    void EmitZeros(FILE* fp, uintptr_t cnt) {
        std::string zero(cnt, '\0');
        WriteBuf(fp, zero.data(), zero.size());
    }

    void EmitPad(FILE* fp, uintptr_t to) {
        uint pos = ftell(fp);
        CHECK(pos >= 0);
        CHECK(pos <= to);
        EmitZeros(fp, to - pos);
    }

    void EmitAlign(FILE* fp) {
        long pos = ftell(fp);
        CHECK(pos >= 0);
        EmitZeros(fp, AlignNext(pos) - pos);
    }

    void Emit(const std::string& out_filename) {
        FILE* fp = fopen(out_filename.c_str(), "wb");
        CHECK(fp);
        Write(fp, ehdr_);
        EmitPhdrs(fp);
        EmitGnuHash(fp);
        EmitSymtab(fp);
        EmitRel(fp);
        EmitArrays(fp);
        EmitStrtab(fp);
        EmitDynamic(fp);
        EmitAlign(fp);

        EmitCode(fp);
        EmitTLS(fp);
        fclose(fp);
    }

    size_t CountPhdrs() const {
        // DYNAMIC and its LOAD.
        size_t num_phdrs = 2;
        // INTERP and PHDR.
        if (is_executable_) num_phdrs += 2;
        // TLS and its LOAD.
        if (tls_.memsz) num_phdrs += 2;
        for (ELFBinary* bin : link_binaries_) {
            num_phdrs += bin->loads().size();
        }
        return num_phdrs;
    }

    uintptr_t GnuHashOffset() const { return sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * ehdr_.e_phnum; }

    uintptr_t SymtabOffset() const { return GnuHashOffset() + syms_.gnu_hash_size(); }

    uintptr_t RelOffset() const { return SymtabOffset() + syms_.size() * sizeof(Elf_Sym); }

    uintptr_t InitArrayOffset() const { return AlignNext(RelOffset() + rels_.size() * sizeof(Elf_Rel), 7); }

    uintptr_t FiniArrayOffset() const { return InitArrayOffset() + sizeof(uintptr_t) * init_array_.size(); }

    uintptr_t StrtabOffset() const { return FiniArrayOffset() + sizeof(uintptr_t) * fini_array_.size(); }

    uintptr_t DynamicOffset() const { return StrtabOffset() + strtab_.size(); }

    uintptr_t CodeOffset() const { return AlignNext(DynamicOffset() + sizeof(Elf_Dyn) * dynamic_.size()); }

    void BuildEhdr() {
        ehdr_ = *main_binary_->ehdr();
        ehdr_.e_entry += offsets_[main_binary_.get()];
        ehdr_.e_shoff = 0;
        ehdr_.e_shnum = 0;
        ehdr_.e_shstrndx = 0;
        ehdr_.e_phnum = CountPhdrs();
    }

    void BuildLoads() {
        uintptr_t file_offset = CodeOffset();
        CHECK(file_offset < offsets_[main_binary_.get()]);
        for (ELFBinary* bin : link_binaries_) {
            uintptr_t offset = offsets_[bin];
            for (Elf_Phdr* phdr : bin->loads()) {
                Load load;
                load.bin = bin;
                load.orig = phdr;
                load.emit = *phdr;

                file_offset += phdr->p_vaddr & 0xfff;
                load.emit.p_offset = file_offset;
                file_offset = AlignNext(file_offset + phdr->p_filesz);
                load.emit.p_vaddr += offset;
                load.emit.p_paddr += offset;
                // TODO(hamaji): Add PF_W only for GOT.
                load.emit.p_flags |= PF_W;
                // TODO(hamaji): Check if this is really safe.
                if (load.emit.p_align > 0x1000) {
                    load.emit.p_align = 0x1000;
                }
                loads_.push_back(load);
            }
        }
        tls_file_offset_ = file_offset;
    }

    void MakeDyn(uint64_t tag, uintptr_t ptr) {
        Elf_Dyn dyn;
        dyn.d_tag = tag;
        dyn.d_un.d_ptr = ptr;
        dynamic_.push_back(dyn);
    }

    void BuildInterp() {
        const std::string interp = main_binary_->head() + main_binary_->GetPhdr(PT_INTERP).p_offset;
        LOGF("Interp: %s\n", interp.c_str());
        interp_offset_ = AddStr(interp);
    }

    void BuildArrays() {
        size_t orig_rel_size = rels_.size();
        for (size_t i = 0; i < init_array_.size() + fini_array_.size(); ++i) {
            rels_.push_back(Elf_Rel{});
        }

        std::vector<uintptr_t> array = init_array_;
        std::copy(fini_array_.begin(), fini_array_.end(), std::back_inserter(array));
        for (size_t i = 0; i < array.size(); ++i) {
            size_t rel_index = orig_rel_size + i;
            CHECK(rel_index < rels_.size());
            Elf_Rel* rel = &rels_[rel_index];
            rel->r_offset = InitArrayOffset() + sizeof(uintptr_t) * i;
            rel->r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
            rel->r_addend = array[i];
        }
    }

    void BuildDynamic() {
        std::set<ELFBinary*> linked(link_binaries_.begin(), link_binaries_.end());
        std::set<std::string> neededs;
        for (const auto& p : libraries_) {
            ELFBinary* bin = p.second.get();
            if (!linked.count(bin)) {
                neededs.insert(bin->soname());
            }
        }

        for (const std::string& needed : neededs) {
            MakeDyn(DT_NEEDED, AddStr(needed));
        }
        if (!main_binary_->rpath().empty()) {
            MakeDyn(DT_RPATH, AddStr(main_binary_->rpath()));
        }
        if (main_binary_->runpath().empty()) {
            MakeDyn(DT_RUNPATH, AddStr(main_binary_->runpath()));
        }

        if (uintptr_t ptr = main_binary_->init()) {
            MakeDyn(DT_INIT, ptr + offsets_[main_binary_.get()]);
        }
        if (uintptr_t ptr = main_binary_->fini()) {
            MakeDyn(DT_FINI, ptr + offsets_[main_binary_.get()]);
        }

        MakeDyn(DT_INIT_ARRAY, InitArrayOffset());
        MakeDyn(DT_INIT_ARRAYSZ, init_array_.size() * sizeof(uintptr_t));
        MakeDyn(DT_FINI_ARRAY, FiniArrayOffset());
        MakeDyn(DT_FINI_ARRAYSZ, fini_array_.size() * sizeof(uintptr_t));

        MakeDyn(DT_GNU_HASH, GnuHashOffset());

        MakeDyn(DT_STRTAB, StrtabOffset());
        MakeDyn(DT_STRSZ, strtab_.size());

        MakeDyn(DT_SYMTAB, SymtabOffset());
        MakeDyn(DT_SYMENT, sizeof(Elf_Sym));

        MakeDyn(DT_RELA, RelOffset());
        MakeDyn(DT_RELAENT, sizeof(Elf_Rel));
        MakeDyn(DT_RELASZ, rels_.size() * sizeof(Elf_Rel));

        MakeDyn(DT_NULL, 0);
    }

    void EmitPhdrs(FILE* fp) {
        std::vector<Elf_Phdr> phdrs;

        CHECK(main_binary_->phdrs().size() > 2);
        if (is_executable_) {
            phdrs.push_back(main_binary_->GetPhdr(PT_PHDR));
            phdrs.push_back(main_binary_->GetPhdr(PT_INTERP));
            phdrs[1].p_offset = phdrs[1].p_vaddr = phdrs[1].p_paddr = StrtabOffset() + interp_offset_;
        }

        size_t dyn_start = DynamicOffset();
        size_t dyn_size = sizeof(Elf_Dyn) * dynamic_.size();
        size_t seg_start = AlignNext(dyn_start + dyn_size);

        {
            Elf_Phdr phdr = main_binary_->GetPhdr(PT_LOAD);
            phdr.p_offset = 0;
            phdr.p_flags = PF_R | PF_W;
            phdr.p_vaddr = 0;
            phdr.p_paddr = 0;
            phdr.p_filesz = seg_start;
            phdr.p_memsz = seg_start;
            phdrs.push_back(phdr);
        }
        {
            Elf_Phdr phdr = main_binary_->GetPhdr(PT_DYNAMIC);
            phdr.p_offset = dyn_start;
            phdr.p_flags = PF_R | PF_W;
            phdr.p_vaddr = dyn_start;
            phdr.p_paddr = dyn_start;
            phdr.p_filesz = dyn_size;
            phdr.p_memsz = dyn_size;
            phdrs.push_back(phdr);
        }

        for (const Load& load : loads_) {
            phdrs.push_back(load.emit);
        }

        if (tls_.memsz) {
            Elf_Phdr phdr;
            phdr.p_offset = tls_file_offset_;
            phdr.p_vaddr = tls_offset_;
            phdr.p_paddr = tls_offset_;
            phdr.p_filesz = tls_.filesz;
            phdr.p_memsz = tls_.memsz;
            phdr.p_align = 0x1000;
            phdr.p_type = PT_TLS;
            phdr.p_flags = PF_R;
            phdrs.push_back(phdr);
            phdr.p_type = PT_LOAD;
            phdr.p_flags = PF_R | PF_W;
            phdrs.push_back(phdr);
        }

        CHECK(phdrs.size() == CountPhdrs());
        for (const Elf_Phdr& phdr : phdrs) {
            Write(fp, phdr);
        }
    }

    void EmitGnuHash(FILE* fp) {
        CHECK(ftell(fp) == GnuHashOffset());
        const Elf_GnuHash& gnu_hash = syms_.gnu_hash();
        Write(fp, gnu_hash.nbuckets);
        Write(fp, gnu_hash.symndx);
        Write(fp, gnu_hash.maskwords);
        Write(fp, gnu_hash.shift2);
        Elf_Addr bloom_filter = -1;
        Write(fp, bloom_filter);
        uint32_t bucket = gnu_hash.symndx;
        Write(fp, bucket);

        const std::vector<std::string>& sym_names = syms_.GetNames();
        for (size_t i = gnu_hash.symndx; i < sym_names.size(); ++i) {
            uint32_t h = CalcGnuHash(sym_names[i]) & ~1;
            if (i == sym_names.size() - 1) {
                h |= 1;
            }
            Write(fp, h);
        }
    }

    void EmitSymtab(FILE* fp) {
        CHECK(ftell(fp) == SymtabOffset());
        for (const Elf_Sym& sym : syms_.Get()) {
            Write(fp, sym);
        }
    }

    void EmitStrtab(FILE* fp) {
        CHECK(ftell(fp) == StrtabOffset());
        WriteBuf(fp, strtab_.data(), strtab_.size());
    }

    void EmitRel(FILE* fp) {
        CHECK(ftell(fp) == RelOffset());
        for (const Elf_Rel& rel : rels_) {
            Write(fp, rel);
        }
    }

    void EmitArrays(FILE* fp) {
        EmitPad(fp, InitArrayOffset());
        for (uintptr_t ptr : init_array_) {
            Write(fp, ptr);
        }
        CHECK(ftell(fp) == FiniArrayOffset());
        for (uintptr_t ptr : fini_array_) {
            Write(fp, ptr);
        }
    }

    void EmitDynamic(FILE* fp) {
        CHECK(ftell(fp) == DynamicOffset());
        for (const Elf_Dyn& dyn : dynamic_) {
            Write(fp, dyn);
        }
    }

    void EmitCode(FILE* fp) {
        for (const Load& load : loads_) {
            ELFBinary* bin = load.bin;
            Elf_Phdr* phdr = load.orig;
            LOGF("Emitting code of %s from %lx => %lx +%lx\n", bin->name().c_str(), ftell(fp), load.emit.p_offset, phdr->p_filesz);
            EmitPad(fp, load.emit.p_offset);
            WriteBuf(fp, bin->head() + phdr->p_offset, phdr->p_filesz);
        }
    }

    void EmitTLS(FILE* fp) {
        EmitPad(fp, tls_file_offset_);
        for (TLS::Data data : tls_.data) {
            WriteBuf(fp, data.start, data.size);
        }
    }

    void DecideOffsets() {
        uintptr_t offset = 0x10000000;
        for (ELFBinary* bin : link_binaries_) {
            const Range range = bin->GetRange() + offset;
            CHECK(range.start == offset);
            offsets_.emplace(bin, range.start);
            LOGF("Assigned: %s %08lx-%08lx\n", bin->soname().c_str(), range.start, range.end);
            offset = range.end;
        }
        tls_offset_ = offset;
    }

    void CollectTLS() {
        uintptr_t bss_offset = 0;
        for (ELFBinary* bin : link_binaries_) {
            for (Elf_Phdr* phdr : bin->phdrs()) {
                if (phdr->p_type == PT_TLS) {
                    uint8_t* start = reinterpret_cast<uint8_t*>(bin->GetPtr(phdr->p_vaddr));
                    size_t size = phdr->p_filesz;
                    uintptr_t file_offset = tls_.filesz;
                    CHECK(tls_.bin_to_index.emplace(bin, tls_.data.size()).second);
                    tls_.data.push_back({bin, start, size, file_offset, bss_offset});
                    tls_.memsz += phdr->p_memsz;
                    tls_.filesz += size;
                    bss_offset += phdr->p_memsz - size;
                }
            }
        }

        for (TLS::Data& d : tls_.data) {
            d.bss_offset += tls_.filesz;
            LOGF("TLS of %s: file=%lx +%lx mem=%lx\n", d.bin->name().c_str(), d.file_offset, d.size, d.bss_offset);
        }

        LOGF("TLS: filesz=%lx memsz=%lx cnt=%zu\n", tls_.filesz, tls_.memsz, tls_.data.size());
    }

    void CollectArrays() {
        for (auto iter = link_binaries_.rbegin(); iter != link_binaries_.rend(); ++iter) {
            ELFBinary* bin = *iter;
            uintptr_t offset = offsets_[bin];
            for (uintptr_t ptr : bin->init_array()) {
                init_array_.push_back(ptr + offset);
            }
        }
        for (ELFBinary* bin : link_binaries_) {
            uintptr_t offset = offsets_[bin];
            for (uintptr_t ptr : bin->fini_array()) {
                fini_array_.push_back(ptr + offset);
            }
        }
        LOGF("Array numbers: init_array=%zu fini_array=%zu\n", init_array_.size(), fini_array_.size());
    }

    void CollectSymbols() {
        std::map<std::string, Elf_Sym*> syms;
        for (ELFBinary* bin : link_binaries_) {
            LoadDynSymtab(bin, &syms);
        }
        LOGF("CollectSymbols\n");
        for (auto it = syms.begin(); it != syms.end(); it++) {
            LOGF("SYM %s\n", it->first.c_str());
        }
        syms_.SetSrcSyms(syms);
    }

    void PrintAllVerneeds() {
        LOGF("PrintAllVerneeds\n");
        for (ELFBinary* bin : link_binaries_) {
            LOGF("==== %s ====\n", bin->filename().c_str());
            bin->PrintVerneeds();
        }
    }

    void PrintAllVersyms() {
        LOGF("PrintAllVersyms\n");
        for (ELFBinary* bin : link_binaries_) {
            LOGF("==== %s ====\n", bin->filename().c_str());
            bin->PrintVersyms();
        }
    }

    uintptr_t RemapTLS(const char* msg, ELFBinary* bin, uintptr_t off) {
        const Elf_Phdr* tls = bin->tls();
        CHECK(tls);
        CHECK(!tls_.data.empty());
        auto found = tls_.bin_to_index.find(bin);
        CHECK(found != tls_.bin_to_index.end());
        const TLS::Data& entry = tls_.data[found->second];
        if (off < tls->p_filesz) {
            LOGF("TLS data %s in %s remapped %lx => %lx\n", msg, bin->name().c_str(), off, off + entry.file_offset);
            off += entry.file_offset;
        } else {
            LOGF("TLS bss %s in %s remapped %lx => %lx\n", msg, bin->name().c_str(), off, off + entry.bss_offset);
            off += entry.bss_offset;
        }
        return off;
    }

    void LoadDynSymtab(ELFBinary* bin, std::map<std::string, Elf_Sym*>* symtab) {
        bin->ReadDynSymtab();

        uintptr_t offset = offsets_[bin];

        for (const auto& p : bin->GetSymbolMap()) {
            const std::string& name = p.first.first;
            Elf_Sym* sym = p.second;
            if (IsTLS(*sym)) {
                sym->st_value = RemapTLS("symbol", bin, sym->st_value);
            } else if (sym->st_value) {
                sym->st_value += offset;
            }
            LOGF("Symbol %s@%s %08lx\n", name.c_str(), bin->name().c_str(), sym->st_value);

            auto inserted = symtab->emplace(name, sym);
            if (!inserted.second) {
                Elf_Sym* sym2 = inserted.first->second;
                int prio = IsDefined(*sym) ? 2 : ELF_ST_BIND(sym->st_info) == STB_WEAK;
                int prio2 = IsDefined(*sym2) ? 2 : ELF_ST_BIND(sym2->st_info) == STB_WEAK;
                if (prio > prio2) {
                    inserted.first->second = sym;
                }
            }
        }
    }

    void CopyPublicSymbols() {
        for (const auto& p : main_binary_->GetSymbolMap()) {
            const Elf_Sym* sym = p.second;
            if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL && IsDefined(*sym)) {
                LOGF("Copy public symbol %s\n", p.first.first.c_str());
                syms_.AddPublicSymbol(p.first.first, *sym);
            }
        }
        for (ELFBinary* bin : link_binaries_) {
            if (bin == main_binary_.get()) continue;
            for (const auto& p : bin->GetSymbolMap()) {
                const Elf_Sym* sym = p.second;
                if (IsTLS(*sym)) {
                    LOGF("Copy TLS symbol %s\n", p.first.first.c_str());
                    syms_.AddPublicSymbol(p.first.first, *sym);
                }
            }
        }
    }

    void Relocate() {
        for (ELFBinary* bin : link_binaries_) {
            RelocateBinary(bin);
        }
    }

    void RelocateBinary(ELFBinary* bin) {
        CHECK(bin->symtab());
        CHECK(bin->rel());
        RelocateSymbols(bin, bin->rel(), bin->num_rels());
        RelocateSymbols(bin, bin->plt_rel(), bin->num_plt_rels());
    }

    void RelocateSymbols(ELFBinary* bin, const Elf_Rel* rels, size_t num) {
        uintptr_t offset = offsets_[bin];
        for (size_t i = 0; i < num; ++i) {
            // TODO(hamaji): Support non-x86-64 architectures.
            RelocateSymbol_x86_64(bin, &rels[i], offset);
        }
    }

    void RelocateSymbol_x86_64(ELFBinary* bin, const Elf_Rel* rel, uintptr_t offset) {
        const Elf_Sym* sym = &bin->symtab()[ELF_R_SYM(rel->r_info)];
        int type = ELF_R_TYPE(rel->r_info);
        const uintptr_t addend = rel->r_addend;
        Elf_Rel newrel = *rel;
        if (bin->InTLS(rel->r_offset)) {
            const Elf_Phdr* tls = bin->tls();
            CHECK(tls);
            uintptr_t off = newrel.r_offset - tls->p_vaddr;
            off = RemapTLS("reloc", bin, off);
            newrel.r_offset = off + tls_offset_;
        } else {
            newrel.r_offset += offset;
        }

        LOGF("Relocate %s at %lx\n", bin->Str(sym->st_name), rel->r_offset);

        switch (type) {
            case R_X86_64_RELATIVE: {
                newrel.r_addend += offset;
                break;
            }

            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT: {
                uintptr_t val_or_index;
                if (syms_.Resolve(bin->Str(sym->st_name), &val_or_index)) {
                    newrel.r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
                    newrel.r_addend = val_or_index;
                } else {
                    newrel.r_info = ELF_R_INFO(val_or_index, type);
                }
                break;
            }

            case R_X86_64_64: {
                uintptr_t val_or_index;
                if (syms_.Resolve(bin->Str(sym->st_name), &val_or_index)) {
                    newrel.r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
                    newrel.r_addend += val_or_index;
                } else {
                    newrel.r_info = ELF_R_INFO(val_or_index, type);
                }
                break;
            }

            case R_X86_64_DTPMOD64:
            case R_X86_64_DTPOFF64:
            case R_X86_64_COPY: {
                const std::string name = bin->Str(sym->st_name);
                uintptr_t index = syms_.ResolveCopy(name);
                newrel.r_info = ELF_R_INFO(index, type);
                break;
            }

            default:
                LOGF("Unknown relocation type: %d\n", type);
                CHECK(false);
        }

        rels_.push_back(newrel);
    }

    void InitLdLibraryPaths() {
        if (const char* paths = getenv("LD_LIBRARY_PATH")) {
            for (const std::string& path : SplitString(paths, ":")) {
                ld_library_paths_.push_back(path);
            }
        }
    }

    std::string ResolveRunPathVariables(const ELFBinary* binary, const std::string& runpath) {
        std::string out = runpath;

        auto replace = [](std::string& s, const std::string& f, const std::string& t) {
            size_t found = s.find(f);
            while (found != std::string::npos) {
                s.replace(found, f.size(), t);
                found = s.find(f);
            }
        };

        std::string origin = binary->filename();
        origin = dirname(&origin[0]);
        replace(out, "$ORIGIN", origin);
        replace(out, "${ORIGIN}", origin);
        if (out.find('$') != std::string::npos) {
            LOGF("Unsupported runpath: %s\n", runpath.c_str());
            abort();
        }
        return out;
    }

    std::vector<std::string> GetLibraryPaths(const ELFBinary* binary) {
        std::vector<std::string> library_paths;
        const std::string& runpath = binary->runpath();
        const std::string& rpath = binary->rpath();
        if (runpath.empty() && !rpath.empty()) {
            for (const std::string& path : SplitString(rpath, ":")) {
                library_paths.push_back(ResolveRunPathVariables(binary, path));
            }
        }
        for (const std::string& path : ld_library_paths_) {
            library_paths.push_back(path);
        }
        if (!runpath.empty()) {
            for (const std::string& path : SplitString(runpath, ":")) {
                library_paths.push_back(ResolveRunPathVariables(binary, path));
            }
        }

        std::vector<std::string> ldsoconfs = ldsoconf::read_ldsoconf();
        library_paths.insert(library_paths.end(), ldsoconfs.begin(), ldsoconfs.end());

        library_paths.push_back("/lib");
        library_paths.push_back("/usr/lib");

        return library_paths;
    }

    void ResolveLibraryPaths(const ELFBinary* binary) {
        std::vector<std::string> library_paths = GetLibraryPaths(binary);

        for (const std::string& needed : binary->neededs()) {
            auto found = libraries_.find(needed);
            if (found != libraries_.end()) {
                continue;
            }

            std::unique_ptr<ELFBinary> library;
            for (const std::string& path : library_paths) {
                const std::string& filename = path + '/' + needed;
                if (Exists(filename)) {
                    library = ReadELF(filename);
                    break;
                }
            }
            if (!library) {
                LOGF("Library %s not found\n", needed.c_str());
                abort();
            }
            if (ShouldLink(library->soname())) {
                link_binaries_.push_back(library.get());
            }

            LOGF("Loaded: %s => %s\n", needed.c_str(), library->filename().c_str());

            auto inserted = libraries_.emplace(needed, std::move(library));
            CHECK(inserted.second);
            ResolveLibraryPaths(inserted.first->second.get());
        }
    }

    bool Exists(const std::string& filename) {
        struct stat st;
        if (stat(filename.c_str(), &st) != 0) {
            return false;
        }
        return (st.st_mode & S_IFMT) & S_IFREG;
    }

    static bool ShouldLink(const std::string& soname) {
        // TODO(hamaji): Make this customizable.
        std::vector<std::string> nolink_prefixes = {
            "libc.so",     "libm.so",      "libdl.so",   "librt.so", "libpthread.so",
            "libgcc_s.so", "libstdc++.so", "libgomp.so", "ld-linux", "libcuda.so",
        };
        for (const std::string& prefix : nolink_prefixes) {
            if (HasPrefix(soname, prefix)) {
                return false;
            }
        }
        return true;
    }

    uintptr_t AddStr(const std::string& s) { return strtab_.Add(s); }

    struct Load {
        ELFBinary* bin;
        Elf_Phdr* orig;
        Elf_Phdr emit;
    };

    std::unique_ptr<ELFBinary> main_binary_;
    std::vector<std::string> ld_library_paths_;
    std::map<std::string, std::unique_ptr<ELFBinary>> libraries_;
    std::vector<ELFBinary*> link_binaries_;
    std::map<ELFBinary*, uintptr_t> offsets_;
    uintptr_t tls_file_offset_{0};
    uintptr_t tls_offset_{0};
    bool is_executable_{false};

    uintptr_t interp_offset_;
    SymtabBuilder syms_;
    std::vector<Elf_Rel> rels_;
    StrtabBuilder strtab_;
    Elf_Ehdr ehdr_;
    std::vector<Load> loads_;
    std::vector<Elf_Dyn> dynamic_;
    std::vector<uintptr_t> init_array_;
    std::vector<uintptr_t> fini_array_;
    TLS tls_;
};

int main(int argc, const char* argv[]) {
    if (argc <= 2) {
        LOGF("Usage: %s <in-elf> <out-elf>\n", argv[0]);
        return 1;
    }

    Sold sold(argv[1]);
    sold.Link(argv[2]);
}
