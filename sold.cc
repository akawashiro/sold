#include <assert.h>
#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
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
#include <iostream>
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
#include "shdr_builder.h"
#include "strtab_builder.h"
#include "symtab_builder.h"
#include "utils.h"
#include "version_builder.h"

class Sold {
public:
    Sold(const std::string& elf_filename, const std::vector<std::string>& exclude_sos) : exclude_sos_(exclude_sos) {
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

        syms_.Build(strtab_, version_);
        syms_.MergePublicSymbols(strtab_, version_);

        // BuildEhdr();
        if (is_executable_) {
            BuildInterp();
        }
        BuildArrays();
        BuildDynamic();

        strtab_.Freeze();
        BuildLoads();

        shdr_.RegisterShdr(ShstrtabOffset(), shdr_.ShstrtabSize(), ShdrBuilder::ShdrType::Shstrtab);
        shdr_.RegisterShdr(DynamicOffset(), DynamicSize(), ShdrBuilder::ShdrType::Dynamic);
        shdr_.RegisterShdr(StrtabOffset(), StrtabSize(), ShdrBuilder::ShdrType::Dynstr);

        // We must call BuildEhdr at the last because of e_shoff
        BuildEhdr();

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
        EmitVersym(fp);
        EmitVerneed(fp);
        EmitRel(fp);
        EmitArrays(fp);
        EmitStrtab(fp);
        EmitDynamic(fp);
        EmitShstrtab(fp);
        EmitAlign(fp);

        EmitCode(fp);
        EmitTLS(fp);

        EmitShdr(fp);

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

    uintptr_t GnuHashOffset() const { return sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * CountPhdrs(); }

    uintptr_t SymtabOffset() const { return GnuHashOffset() + syms_.GnuHashSize(); }

    uintptr_t VersymOffset() const { return SymtabOffset() + syms_.size() * sizeof(Elf_Sym); }

    uintptr_t VerneedOffset() const { return VersymOffset() + version_.SizeVersym(); }

    uintptr_t RelOffset() const { return VerneedOffset() + version_.SizeVerneed(); }

    uintptr_t InitArrayOffset() const { return AlignNext(RelOffset() + rels_.size() * sizeof(Elf_Rel), 7); }

    uintptr_t FiniArrayOffset() const { return InitArrayOffset() + sizeof(uintptr_t) * init_array_.size(); }

    uintptr_t StrtabOffset() const { return FiniArrayOffset() + sizeof(uintptr_t) * fini_array_.size(); }
    uintptr_t StrtabSize() const { return strtab_.size(); }

    uintptr_t DynamicOffset() const { return StrtabOffset() + StrtabSize(); }
    uintptr_t DynamicSize() const { return sizeof(Elf_Dyn) * dynamic_.size(); }

    uintptr_t ShstrtabOffset() const { return DynamicOffset() + DynamicSize(); }

    uintptr_t CodeOffset() const { return AlignNext(ShstrtabOffset() + shdr_.ShstrtabSize()); }

    uintptr_t TLSOffset() const { return tls_file_offset_; }
    uintptr_t TLSSize() const {
        uintptr_t s = 0;
        for (const TLS::Data& data : tls_.data) {
            s += data.size;
        }
        return s;
    }

    uintptr_t ShdrOffset() const { return TLSOffset() + TLSSize(); }

    // You must call this function after building all stuffs
    // because ShdrOffset() is not fixed before it.
    void BuildEhdr() {
        ehdr_ = *main_binary_->ehdr();
        ehdr_.e_entry += offsets_[main_binary_.get()];
        ehdr_.e_shoff = ShdrOffset();
        ehdr_.e_shnum = shdr_.CountShdrs();
        ehdr_.e_shstrndx = shdr_.Shstrndx();
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

        MakeDyn(DT_VERSYM, VersymOffset());
        MakeDyn(DT_VERNEEDNUM, version_.NumVerneed());
        MakeDyn(DT_VERNEED, VerneedOffset());

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
        const std::vector<Syminfo>& exposed_syms = syms_.GetExposedSyms();

        Write(fp, gnu_hash.nbuckets);
        Write(fp, gnu_hash.symndx);
        Write(fp, gnu_hash.maskwords);
        Write(fp, gnu_hash.shift2);
        Elf_Addr bloom_filter = -1;
        Write(fp, bloom_filter);
        // If there is no symbols in gnu_hash_, bucket must be 0.
        uint32_t bucket = (exposed_syms.size() > gnu_hash.symndx) ? gnu_hash.symndx : 0;
        Write(fp, bucket);

        for (size_t i = gnu_hash.symndx; i < exposed_syms.size(); ++i) {
            uint32_t h = CalcGnuHash(exposed_syms[i].name) & ~1;
            if (i == exposed_syms.size() - 1) {
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

    void EmitVersym(FILE* fp) {
        CHECK(ftell(fp) == VersymOffset());
        version_.EmitVersym(fp);
    }

    void EmitVerneed(FILE* fp) {
        CHECK(ftell(fp) == VerneedOffset());
        version_.EmitVerneed(fp, strtab_);
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

    void EmitShstrtab(FILE* fp) {
        CHECK(ftell(fp) == ShstrtabOffset());
        shdr_.EmitShstrtab(fp);
    }

    void EmitDynamic(FILE* fp) {
        CHECK(ftell(fp) == DynamicOffset());
        for (const Elf_Dyn& dyn : dynamic_) {
            Write(fp, dyn);
        }
    }

    void EmitCode(FILE* fp) {
        CHECK(ftell(fp) == CodeOffset());
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
        CHECK(ftell(fp) == TLSOffset());
        for (TLS::Data data : tls_.data) {
            WriteBuf(fp, data.start, data.size);
        }
    }

    void EmitShdr(FILE* fp) {
        CHECK(ftell(fp) == ShdrOffset());
        shdr_.EmitShdrs(fp);
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
        std::vector<Syminfo> syms;
        for (ELFBinary* bin : link_binaries_) {
            LoadDynSymtab(bin, syms);
        }
        LOGF("CollectSymbols\n");
        for (auto s : syms) {
            LOGF("SYM %s\n", s.name.c_str());
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

    void LoadDynSymtab(ELFBinary* bin, std::vector<Syminfo>& symtab) {
        bin->ReadDynSymtab();

        uintptr_t offset = offsets_[bin];

        for (const auto& p : bin->GetSymbolMap()) {
            const std::string& name = p.name;
            Elf_Sym* sym = p.sym;
            if (IsTLS(*sym)) {
                sym->st_value = RemapTLS("symbol", bin, sym->st_value);
            } else if (sym->st_value) {
                sym->st_value += offset;
            }
            LOGF("Symbol %s@%s %08lx\n", name.c_str(), bin->name().c_str(), sym->st_value);

            Syminfo* found = NULL;
            for (int i = 0; i < symtab.size(); i++) {
                if (symtab[i].name == p.name && symtab[i].soname == p.soname && symtab[i].version == p.version) {
                    found = &symtab[i];
                    break;
                }
            }

            if (found == NULL) {
                symtab.push_back(p);
            } else {
                Elf_Sym* sym2 = found->sym;
                int prio = IsDefined(*sym) ? 2 : ELF_ST_BIND(sym->st_info) == STB_WEAK;
                int prio2 = IsDefined(*sym2) ? 2 : ELF_ST_BIND(sym2->st_info) == STB_WEAK;
                if (prio > prio2) {
                    found->sym = sym;
                }
            }
        }
    }

    void CopyPublicSymbols() {
        for (const auto& p : main_binary_->GetSymbolMap()) {
            const Elf_Sym* sym = p.sym;
            if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL && IsDefined(*sym)) {
                LOGF("Copy public symbol %s\n", p.name.c_str());
                syms_.AddPublicSymbol(p);
            }
        }
        for (ELFBinary* bin : link_binaries_) {
            if (bin == main_binary_.get()) continue;
            for (const auto& p : bin->GetSymbolMap()) {
                const Elf_Sym* sym = p.sym;
                if (IsTLS(*sym)) {
                    LOGF("Copy TLS symbol %s\n", p.name.c_str());
                    syms_.AddPublicSymbol(p);
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
        auto [filename, version_name] = bin->GetVerneed(ELF_R_SYM(rel->r_info));

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
                if (syms_.Resolve(bin->Str(sym->st_name), filename, version_name, val_or_index)) {
                    newrel.r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
                    newrel.r_addend = val_or_index;
                } else {
                    newrel.r_info = ELF_R_INFO(val_or_index, type);
                }
                break;
            }

            case R_X86_64_64: {
                uintptr_t val_or_index;
                if (syms_.Resolve(bin->Str(sym->st_name), filename, version_name, val_or_index)) {
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
                uintptr_t index = syms_.ResolveCopy(name, filename, version_name);
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
                    if (library) break;
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

    bool ShouldLink(const std::string& soname) {
        std::vector<std::string> nolink_prefixes = {"libc.so",     "libm.so",      "libdl.so",   "librt.so", "libpthread.so",
                                                    "libgcc_s.so", "libstdc++.so", "libgomp.so", "ld-linux", "libcuda.so"};
        for (const std::string& prefix : nolink_prefixes) {
            if (HasPrefix(soname, prefix)) {
                return false;
            }
        }
        for (const std::string& prefix : exclude_sos_) {
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
    std::vector<std::string> exclude_sos_;
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
    VersionBuilder version_;
    ShdrBuilder shdr_;
    Elf_Ehdr ehdr_;
    std::vector<Load> loads_;
    std::vector<Elf_Dyn> dynamic_;
    std::vector<uintptr_t> init_array_;
    std::vector<uintptr_t> fini_array_;
    TLS tls_;
};

void print_help(std::ostream& os) {
    os << R"(usage: sold [option] [input]
Options:
-h, --help                      Show this help message and exit
-o, --output-file OUTPUT_FILE   Specify the ELF file to output (this option is mandatory)
-i, --input-file INPUT_FILE     Specify the ELF file to output
-e, --exclude-so EXCLUDE_FILE   Specify the ELF file to exclude (e.g. libmax.so) 

The last argument is interpreted as SOURCE_FILE when -i option isn't given.
)" << std::endl;
}

int main(int argc, char* const argv[]) {
    static option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"input-file", required_argument, nullptr, 'i'},
        {"output-file", required_argument, nullptr, 'o'},
        {"exclude-so", required_argument, nullptr, 'e'},
        {0, 0, 0, 0},
    };

    std::string input_file;
    std::string output_file;
    std::vector<std::string> exclude_sos;

    int opt;
    while ((opt = getopt_long(argc, argv, "hi:o:e:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'e':
                exclude_sos.push_back(optarg);
                break;
            case 'i':
                input_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'h':
                print_help(std::cout);
                return 0;
            case '?':
                print_help(std::cerr);
                return 1;
        }
    }

    if (optind < argc && input_file.empty()) {
        input_file = argv[optind++];
    }

    if (output_file == "") {
        std::cerr << "You must specify the output file." << std::endl;
        return 1;
    }

    Sold sold(input_file, exclude_sos);
    sold.Link(output_file);
}
