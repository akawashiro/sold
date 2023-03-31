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

#include "sold.h"

#include <algorithm>
#include <list>
#include <queue>
#include <set>

Sold::Sold(const std::string& elf_filename, const std::vector<std::string>& exclude_sos, const std::vector<std::string>& exclude_dirs,
           const std::vector<std::string>& exclude_finis, const std::vector<std::string> custome_library_path,
           const std::vector<std::string>& exclude_runpath_pattern, bool emit_section_header, bool delete_unused_DT_STRTAB)
    : exclude_sos_(exclude_sos),
      exclude_dirs_(exclude_dirs),
      exclude_finis_(exclude_finis),
      custome_library_path_(custome_library_path),
      exclude_runpath_pattern_(exclude_runpath_pattern),
      emit_section_header_(emit_section_header),
      delete_unused_DT_STRTAB_(delete_unused_DT_STRTAB) {
    main_binary_ = ReadELF(elf_filename);
    is_executable_ = main_binary_->FindPhdr(PT_INTERP);
    machine_type = main_binary_->ehdr()->e_machine;
    memprotect_builder_.SetMachineType(machine_type);

    // Register (filename, soname) of main_binary_
    if (main_binary_->name() != "" && main_binary_->soname() != "") {
        filename_to_soname_[main_binary_->name()] = main_binary_->soname();
        soname_to_filename_[main_binary_->soname()] = main_binary_->name();
        LOG(INFO) << SOLD_LOG_KEY(main_binary_->name()) << SOLD_LOG_KEY(main_binary_->soname());
    } else {
        // soname of main_binary_ can be empty when main_binary_ is executable file.
        LOG(WARNING) << "Empty filename or soname: " << SOLD_LOG_KEY(main_binary_->name()) << SOLD_LOG_KEY(main_binary_->soname());
    }

    InitLdLibraryPaths();
    ResolveLibraryPaths(main_binary_.get());

    version_.SetSonameToFilename(soname_to_filename_);
}

void Sold::Link(const std::string& out_filename) {
    DecideMemOffset();

    CollectTLS();
    CollectArrays();
    CollectSymbols();
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
    BuildMprotect();

    strtab_.Freeze();
    BuildLoads();
    BuildEHFrameHeader();

    shdr_.RegisterShdr(GnuHashOffset(), GnuHashSize(), ShdrBuilder::ShdrType::GnuHash);
    shdr_.RegisterShdr(SymtabOffset(), SymtabSize(), ShdrBuilder::ShdrType::Dynsym, sizeof(Elf_Sym));
    if (version_.NumVerneed() > 0) {
        shdr_.RegisterShdr(VersymOffset(), VersymSize(), ShdrBuilder::ShdrType::GnuVersion, sizeof(Elf_Versym));
        shdr_.RegisterShdr(VerneedOffset(), VerneedSize(), ShdrBuilder::ShdrType::GnuVersionR, 0, version_.NumVerneed());
    }
    shdr_.RegisterShdr(RelOffset(), RelSize(), ShdrBuilder::ShdrType::RelaDyn, sizeof(Elf_Rel));
    shdr_.RegisterShdr(InitArrayOffset(), InitArraySize(), ShdrBuilder::ShdrType::InitArray);
    shdr_.RegisterShdr(FiniArrayOffset(), FiniArraySize(), ShdrBuilder::ShdrType::FiniArray);
    shdr_.RegisterShdr(StrtabOffset(), StrtabSize(), ShdrBuilder::ShdrType::Dynstr);
    shdr_.RegisterShdr(DynamicOffset(), DynamicSize(), ShdrBuilder::ShdrType::Dynamic, sizeof(Elf_Dyn));
    shdr_.RegisterShdr(ShstrtabOffset(), ShstrtabSize(), ShdrBuilder::ShdrType::Shstrtab);
    // TODO(akawashiro) .text and .tls
    shdr_.Freeze();

    // We must call BuildEhdr at the last because of e_shoff
    BuildEhdr();

    Emit(out_filename);
    CHECK(chmod(out_filename.c_str(), 0755) == 0);
}

void Sold::Emit(const std::string& out_filename) {
    FILE* fp = fopen(out_filename.c_str(), "wb");
    CHECK(fp);
    Write(fp, ehdr_);
    EmitPhdrs(fp);
    EmitArrays(fp);
    EmitGnuHash(fp);
    EmitSymtab(fp);
    EmitVersym(fp);
    EmitVerneed(fp);
    EmitRel(fp);
    EmitStrtab(fp);
    EmitDynamic(fp);
    EmitShstrtab(fp);
    EmitAlign(fp);

    EmitCode(fp);
    EmitTLS(fp);
    EmitEHFrame(fp);
    EmitMemprotect(fp);

    if (emit_section_header_) EmitShdr(fp);

    fclose(fp);
}

// You must call this function after building all stuffs
// because ShdrOffset() cannot be fixed before it.
void Sold::BuildEhdr() {
    ehdr_ = *main_binary_->ehdr();
    ehdr_.e_entry += offsets_[main_binary_.get()];
    ehdr_.e_shoff = ShdrOffset();
    ehdr_.e_shnum = shdr_.CountShdrs();
    ehdr_.e_shstrndx = shdr_.Shstrndx();
    ehdr_.e_phnum = CountPhdrs();
}

void Sold::BuildLoads() {
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
    file_offset = AlignNext(file_offset + TLSFileSize());
    ehframe_file_offset_ = file_offset;
    file_offset = AlignNext(file_offset + EHFrameSize());
    mprotect_file_offset_ = file_offset;

    for (const Load& load : loads_) {
        LOG(INFO) << "PT_LOAD mapping: name=" << load.bin->name() << " vaddr=" << load.emit.p_vaddr << " memsz=" << load.emit.p_memsz
                  << " offset=" << load.emit.p_offset << " filesz=" << load.emit.p_filesz << " orig_vaddr=" << load.orig->p_vaddr
                  << " orig_offset=" << load.orig->p_offset;
    }
}

void Sold::BuildArrays() {
    std::vector<uintptr_t> array = init_array_;
    std::copy(fini_array_.begin(), fini_array_.end(), std::back_inserter(array));

    // We must emit a relocation because the first element of init_array_ is
    // mprotect_offset_.
    CHECK_GE(init_array_.size(), 1);
    Elf_Rel mprotect_rel;
    if (machine_type == EM_X86_64) {
        mprotect_rel.r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
    } else if (machine_type == EM_AARCH64) {
        mprotect_rel.r_info = ELF_R_INFO(0, R_AARCH64_RELATIVE);
    } else {
        CHECK(false);
    }
    mprotect_rel.r_offset = InitArrayOffset();
    mprotect_rel.r_addend = array[0];
    rels_.emplace_back(mprotect_rel);
}

std::string Sold::BuildRunpath() {
    std::string runpath;
    for (const ELFBinary* b : link_binaries_) {
        std::vector<std::string> runpaths = SplitString(b->runpath(), ":");
        for (const auto rp : runpaths) {
            bool matched = false;
            for (const auto pattern : exclude_runpath_pattern_) {
                if (rp.find(pattern) != std::string::npos) matched = true;
            }
            if (!matched) {
                runpath += rp + ":";
            }
        }
    }
    return runpath;
}

void Sold::BuildDynamic() {
    std::set<ELFBinary*> linked(link_binaries_.begin(), link_binaries_.end());
    std::set<std::string> neededs;
    for (const auto& p : libraries_) {
        ELFBinary* bin = p.second.get();
        if (!linked.count(bin)) {
            neededs.insert(bin->name());
        }
    }

    for (const std::string& needed : neededs) {
        MakeDyn(DT_NEEDED, AddStr(needed));
    }
    if (!main_binary_->soname().empty()) {
        MakeDyn(DT_SONAME, AddStr(main_binary_->soname()));
    }
    if (!main_binary_->rpath().empty()) {
        MakeDyn(DT_RPATH, AddStr(main_binary_->rpath()));
    }
    if (!main_binary_->runpath().empty()) {
        MakeDyn(DT_RUNPATH, AddStr(BuildRunpath()));
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

    if (version_.NumVerneed() > 0) {
        MakeDyn(DT_VERSYM, VersymOffset());
        MakeDyn(DT_VERNEEDNUM, version_.NumVerneed());
        MakeDyn(DT_VERNEED, VerneedOffset());
    }

    MakeDyn(DT_RELA, RelOffset());
    MakeDyn(DT_RELAENT, sizeof(Elf_Rel));
    MakeDyn(DT_RELASZ, rels_.size() * sizeof(Elf_Rel));

    MakeDyn(DT_NULL, 0);
}

void Sold::EmitPhdrs(FILE* fp) {
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
    {
        Elf_Phdr phdr;
        phdr.p_offset = ehframe_file_offset_;
        phdr.p_vaddr = ehframe_offset_;
        phdr.p_paddr = ehframe_offset_;
        phdr.p_filesz = ehframe_builder_.Size();
        phdr.p_memsz = ehframe_builder_.Size();
        phdr.p_align = 0x1000;
        phdr.p_type = PT_GNU_EH_FRAME;
        phdr.p_flags = PF_R;
        phdrs.push_back(phdr);
        phdr.p_type = PT_LOAD;
        phdr.p_flags = PF_R | PF_W;
        phdrs.push_back(phdr);
    }
    {
        Elf_Phdr phdr;
        phdr.p_offset = mprotect_file_offset_;
        phdr.p_vaddr = mprotect_offset_;
        phdr.p_paddr = mprotect_offset_;
        phdr.p_filesz = MprotectSize();
        phdr.p_memsz = MprotectSize();
        phdr.p_align = 0x1000;
        phdr.p_type = PT_LOAD;
        phdr.p_flags = PF_R | PF_X;
        phdrs.push_back(phdr);
    }
    {
        Elf_Phdr phdr;
        phdr.p_offset = 0;
        phdr.p_vaddr = 0;
        phdr.p_paddr = 0;
        phdr.p_filesz = 0;
        phdr.p_memsz = 0;
        phdr.p_align = 0x10;  // TODO(akawashiro) Is it appropriate?
        phdr.p_type = PT_GNU_STACK;
        phdr.p_flags = PF_R | PF_W;
        for (ELFBinary* bin : link_binaries_) {
            if (bin->gnu_stack() != NULL) {
                phdr.p_flags |= bin->gnu_stack()->p_flags;
            }
        }
        phdrs.push_back(phdr);
    }

    CHECK(phdrs.size() == CountPhdrs());
    for (const Elf_Phdr& phdr : phdrs) {
        Write(fp, phdr);
    }
}

void Sold::EmitGnuHash(FILE* fp) {
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

uintptr_t Sold::TLSMemSize() const {
    static uintptr_t s = 0;
    if (s != 0) return s;
    for (ELFBinary* bin : link_binaries_) {
        for (Elf_Phdr* phdr : bin->phdrs()) {
            if (phdr->p_type == PT_TLS) {
                s += phdr->p_memsz;
            }
        }
    }
    return s;
}

// Decide locations for each linked shared objects
// TODO(akawashiro) Is the initial value of offset optimal?
void Sold::DecideMemOffset() {
    uintptr_t offset = 0x10000000;
    for (ELFBinary* bin : link_binaries_) {
        const Range range = bin->GetRange() + offset;
        CHECK(range.start == offset) << "sold cannot handle other than shared objects.";
        offsets_.emplace(bin, range.start);
        LOG(INFO) << "Assigned: " << bin->soname() << " " << HexString(range.start, 8) << "-" << HexString(range.end, 8);
        offset = range.end;
    }
    tls_offset_ = offset;
    offset = AlignNext(offset + TLSMemSize());
    ehframe_offset_ = offset;
    offset = AlignNext(offset + EHFrameSize());
    mprotect_offset_ = offset;
    offset = AlignNext(offset + MprotectSize());
}

void Sold::CollectTLS() {
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
    SOLD_CHECK_EQ(tls_.memsz, TLSMemSize());

    for (TLS::Data& d : tls_.data) {
        d.bss_offset += tls_.filesz;
        LOG(INFO) << "TLS of " << d.bin->name() << ": file=" << HexString(d.file_offset) << " + " << HexString(d.size)
                  << " mem=" << HexString(d.bss_offset);
    }

    LOG(INFO) << "TLS: filesz=" << HexString(tls_.filesz) << " memsz=" << HexString(tls_.memsz) << " cnt=" << HexString(tls_.data.size());
}

// Collect .init_array and .fini_array
void Sold::CollectArrays() {
    init_array_.emplace_back(mprotect_offset_);
    for (auto iter = link_binaries_.rbegin(); iter != link_binaries_.rend(); ++iter) {
        ELFBinary* bin = *iter;
        uintptr_t offset = offsets_[bin];
        bin_to_init_array_offset_[bin] = InitArrayOffset() + sizeof(uintptr_t) * init_array_.size();
        for (uintptr_t ptr : bin->init_array()) {
            init_array_.emplace_back(ptr + offset);
        }
    }
    for (ELFBinary* bin : link_binaries_) {
        uintptr_t offset = offsets_[bin];
        if (std::any_of(exclude_finis_.cbegin(), exclude_finis_.cend(), [bin](const auto s) { return HasPrefix(bin->soname(), s); }))
            continue;
        bin_to_fini_array_offset_[bin] = FiniArrayOffset() + sizeof(uintptr_t) * fini_array_.size();
        for (uintptr_t ptr : bin->fini_array()) {
            fini_array_.emplace_back(ptr + offset);
        }
    }
    LOG(INFO) << "Array numbers: init_array=" << init_array_.size() << " fini_array=" << fini_array_.size();
}

uintptr_t Sold::RemapTLS(const char* msg, ELFBinary* bin, uintptr_t off) {
    const Elf_Phdr* tls = bin->tls();
    CHECK(tls);
    CHECK(!tls_.data.empty());
    auto found = tls_.bin_to_index.find(bin);
    CHECK(found != tls_.bin_to_index.end());
    const TLS::Data& entry = tls_.data[found->second];
    if (off < tls->p_filesz) {
        LOG(INFO) << "TLS data " << msg << " in " << bin->name() << " remapped " << HexString(off) << " => "
                  << HexString(off + entry.file_offset);
        off += entry.file_offset;
    } else {
        LOG(INFO) << "TLS bss " << msg << " in " << bin->name() << " remapped " << HexString(off) << " => "
                  << HexString(off + entry.bss_offset);
        off += entry.bss_offset;
    }
    return off;
}

// Push symbols of bin to symtab.
// When the same symbol is already in symtab, LoadDynSymtab selects a more
// concretely defined one.
void Sold::LoadDynSymtab(ELFBinary* bin, std::vector<Syminfo>& symtab) {
    bin->ReadDynSymtab(filename_to_soname_);

    uintptr_t offset = offsets_[bin];

    for (const auto& p : bin->GetSymbolMap()) {
        const std::string& name = p.name;
        Elf_Sym* sym = p.sym;
        if (IsTLS(*sym) && sym->st_shndx != SHN_UNDEF) {
            sym->st_value = RemapTLS("symbol", bin, sym->st_value);
        } else if (sym->st_value) {
            sym->st_value += offset;
        }
        LOG(INFO) << "Symbol " << name << "@" << bin->name() << SOLD_LOG_KEY(p.soname) << SOLD_LOG_KEY(p.version) << SOLD_LOG_KEY(sym->st_value);

        Syminfo* found = NULL;
        for (int i = 0; i < symtab.size(); i++) {
            if (symtab[i].name == p.name && symtab[i].soname == p.soname && symtab[i].version == p.version) {
                found = &symtab[i];
                break;
            }
            // Also catch cases where symtab[i] is VER_NDX_LOCAL and p has specific version.
            if (symtab[i].name == p.name && symtab[i].versym == VER_NDX_LOCAL) {
                CHECK(symtab[i].soname.empty() && symtab[i].version.empty());
                found = &symtab[i];
                LOG(INFO) << "Found VER_NDX_LOCAL symbol " << symtab[i].name << " and versioned one" << SOLD_LOG_KEY(p.soname) << SOLD_LOG_KEY(p.version);
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
                *found = p;
            }
            LOG(INFO) << "Found " << SOLD_LOG_KEY(p.name) << " twice" << SOLD_LOG_KEY(prio) << SOLD_LOG_KEY(prio2);

            if (prio == 2 && prio2 == 2) {
                LOG(INFO) << "Symbol " << SOLD_LOG_KEY(p.name) << SOLD_LOG_KEY(p.soname) << SOLD_LOG_KEY(p.version)
                          << " is defined in two shared objects.";
            }
        }
    }
}

// Push all global symbols of main_binary_ into public_syms_.
// Push all TLS symbols into public_syms_.
// TODO(akawashiro) Does public_syms_ overlap with exposed_syms_?
void Sold::CopyPublicSymbols() {
    for (const auto& p : main_binary_->GetSymbolMap()) {
        const Elf_Sym* sym = p.sym;

        // TODO(akawashiro) Do we need this IsDefined check?
        if ((ELF_ST_BIND(sym->st_info) == STB_GLOBAL || ELF_ST_BIND(sym->st_info) == STB_WEAK) && IsDefined(*sym)) {
            LOG(INFO) << "Copy public symbol " << SOLD_LOG_KEY(p);
            syms_.AddPublicSymbol(p);
        } else {
            LOG(INFO) << "Don't copy public symbol " << SOLD_LOG_KEY(p);
        }
    }
    for (ELFBinary* bin : link_binaries_) {
        if (bin == main_binary_.get()) continue;
        for (const auto& p : bin->GetSymbolMap()) {
            const Elf_Sym* sym = p.sym;
            if (IsTLS(*sym)) {
                LOG(INFO) << "Copy TLS symbol " << p.name;
                syms_.AddPublicSymbol(p);
            }
        }
    }
}

// Make new relocation table.
// RelocateSymbol_x86_64 rewrites r_offset of each relocation entries
// because we decided locations of shared objects in DecideMemOffset.
void Sold::RelocateSymbol_x86_64(ELFBinary* bin, const Elf_Rel* rel, uintptr_t offset) {
    const Elf_Sym* sym = &bin->symtab()[ELF_R_SYM(rel->r_info)];
    std::string soname, version_name;
    std::tie(soname, version_name) = bin->GetVersion(ELF_R_SYM(rel->r_info), filename_to_soname_);

    int type = ELF_R_TYPE(rel->r_info);
    const uintptr_t addend = rel->r_addend;
    std::vector<Elf_Rel> newrels;

    if (bin->IsVaddrInTLSData(rel->r_offset)) {
        Elf_Rel newrel = *rel;
        const Elf_Phdr* tls = bin->tls();
        CHECK(tls);
        uintptr_t off = newrel.r_offset - tls->p_vaddr;
        off = RemapTLS("reloc", bin, off);
        newrel.r_offset = off + tls_offset_;
        newrels.emplace_back(newrel);
    } else {
        Elf_Rel newrel = *rel;
        newrel.r_offset += offset;
        newrels.emplace_back(newrel);
    }

    if (bin->IsAddrInInitarray(rel->r_offset)) {
        Elf_Rel newrel = *rel;
        CHECK(bin_to_init_array_offset_.find(bin) != bin_to_init_array_offset_.end()) << SOLD_LOG_KEY(bin->filename());

        newrel.r_offset -= bin->init_array_addr();
        newrel.r_offset += bin_to_init_array_offset_[bin];
        LOG(INFO) << SOLD_LOG_BITS(bin->init_array_addr()) << SOLD_LOG_BITS(bin_to_init_array_offset_[bin])
                  << SOLD_LOG_BITS(newrel.r_offset) << SOLD_LOG_BITS(newrel.r_addend) << SOLD_LOG_BITS(offset);
        newrels.emplace_back(newrel);
    } else if (bin->IsAddrInFiniarray(rel->r_offset)) {
        Elf_Rel newrel = *rel;
        CHECK(bin_to_fini_array_offset_.find(bin) != bin_to_fini_array_offset_.end()) << SOLD_LOG_KEY(bin->filename());

        newrel.r_offset -= bin->fini_array_addr();
        newrel.r_offset += bin_to_fini_array_offset_[bin];
        LOG(INFO) << SOLD_LOG_BITS(bin->fini_array_addr()) << SOLD_LOG_BITS(bin_to_fini_array_offset_[bin])
                  << SOLD_LOG_BITS(newrel.r_offset) << SOLD_LOG_BITS(newrel.r_addend) << SOLD_LOG_BITS(offset);
        newrels.emplace_back(newrel);
    }

    VLOG(3) << "Relocate " << bin->Str(sym->st_name) << " at " << rel->r_offset;

    for (auto newrel : newrels) {
        // Even if we found a defined symbol in src_syms_, we cannot
        // erase the relocation entry. The address needs to be fixed at
        // runtime by ASLR function so we set RELATIVE to these resolved symbols.
        switch (type) {
            case R_X86_64_RELATIVE: {
                if (IsDefined(*sym)) {
                    LOG(WARNING)
                        << "The symbol associated with R_X86_64_RELATIVE is defined. Because this relocation type doesn't need any "
                           "symbol, something wrong may have happened.";
                }
                newrel.r_addend += offset;
                break;
            }

            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT: {
                uintptr_t val_or_index;
                if (syms_.Resolve(bin->Str(sym->st_name), soname, version_name, val_or_index)) {
                    newrel.r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
                    newrel.r_addend = val_or_index;
                } else {
                    newrel.r_info = ELF_R_INFO(val_or_index, type);
                }
                break;
            }

            case R_X86_64_64: {
                uintptr_t val_or_index;
                if (syms_.Resolve(bin->Str(sym->st_name), soname, version_name, val_or_index)) {
                    newrel.r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
                    newrel.r_addend += val_or_index;
                } else {
                    newrel.r_info = ELF_R_INFO(val_or_index, type);
                }
                break;
            }

            // TODO(akawashiro) Handle TLS variables in executables.
            case R_X86_64_DTPMOD64: {
                // TODO(akawashiro) Refactor out for Arch64
                const std::string name = bin->Str(sym->st_name);
                uintptr_t index = syms_.ResolveCopy(name, soname, version_name);
                newrel.r_info = ELF_R_INFO(index, type);

                if (bin->tls() == NULL) {
                    LOG(INFO) << SOLD_LOG_64BITS(bin->tls()) << " is null. This relocation is TLS generic dynamic model.";
                    break;
                }

                uint64_t* mod_on_got = reinterpret_cast<uint64_t*>(bin->head_mut() + bin->OffsetFromAddr(rel->r_offset));
                uint64_t* offset_on_got = mod_on_got + 1;
                const bool is_bss = bin->IsOffsetInTLSBSS(*offset_on_got);

                // We assume dl_tls_index exists in GOT. This struct is used as
                // the argument of __tls_get_addr.
                //
                // typedef struct dl_tls_index
                // {
                //   uint64_t ti_module; <--- mod_on_got
                //   uint64_t ti_offset; <--- offset_on_got
                // } tls_index;
                //
                // In TLS generic dynamic model, both ti_module and ti_offset are
                // rewritten by R_X86_64_DTPMOD64 and R_X86_64_DTPOFF64,
                // respectively.
                //
                // In TLS local dynamic model, the only ti_module is rewrite by
                // R_X86_64_DTPMOD64 and ti_offset is fixed in the link process. We
                // must rewrite the fixed ti_offset because we remap the TLS
                // template.

                std::vector<int> rewrite_rel_types;  // Type of relocations which rewrite ti_module.
                for (size_t i = 0; i < bin->num_rels(); ++i) {
                    if (rel->r_offset + sizeof(uint64_t) <= bin->rel()[i].r_offset &&
                        bin->rel()[i].r_offset < rel->r_offset + sizeof(uint64_t) + sizeof(uint64_t)) {
                        rewrite_rel_types.emplace_back(ELF_R_TYPE(bin->rel()[i].r_info));
                    }
                }

                CHECK(rewrite_rel_types.size() == 0 || (rewrite_rel_types.size() == 1 && rewrite_rel_types[0] == R_X86_64_DTPOFF64))
                    << SOLD_LOG_KEY(rewrite_rel_types.size()) << SOLD_LOG_KEY(ShowRelocationType(rewrite_rel_types[0]));

                if (rewrite_rel_types.size() == 1) {
                    LOG(INFO) << "R_X86_64_DTPOFF64 exists next to R_X86_64_DTPMOD64. This relocation is TLS generic dynamic model.";
                    break;
                }

                LOG(INFO) << "R_X86_64_DTPMOD64 relocation in TLS local dynamic model. " << SOLD_LOG_KEY(*rel) << SOLD_LOG_KEY(newrel)
                          << SOLD_LOG_64BITS(bin->OffsetFromAddr(rel->r_offset)) << SOLD_LOG_64BITS(*mod_on_got)
                          << SOLD_LOG_64BITS(*offset_on_got) << SOLD_LOG_64BITS(bin->tls()->p_filesz) << SOLD_LOG_KEY(is_bss)
                          << SOLD_LOG_64BITS(tls_.data[tls_.bin_to_index[bin]].file_offset)
                          << SOLD_LOG_64BITS(tls_.data[tls_.bin_to_index[bin]].bss_offset);

                // We cannot determine whether the associated symbol is a dummy or
                // not just using its index. In addition to the traditional dummy
                // symbol at index 0, I found some compilers emit a dummy symbol at
                // index 1 of SECTION type.
                CHECK_EQ(name, "") << "The symbol associated with R_X86_64_DTPMOD64 in TLS local dynamic model should be the dummy."
                                   << SOLD_LOG_KEY(bin->filename());

                if (is_bss) {
                    // TLS variables without initial values are remapped from
                    // [bin->tls()->p_filesz, bin->tls()->p_memsz) to
                    // [tls_.data[tls_.bin_to_index[bin]].bss_offset,
                    //  tls_.data[tls_.bin_to_index[bin]].bss_offset + bin->tls()->p_memsz - bin->tls()->p_filesz)
                    *offset_on_got += tls_.data[tls_.bin_to_index[bin]].bss_offset - bin->tls()->p_filesz;
                } else {
                    // TLS variables with initial values are remapped from
                    // [0, bin->tls()->p_filesz) to
                    // [tls_.data[tls_.bin_to_index[bin]].file_offset,
                    //  tls_.data[tls_.bin_to_index[bin]].file_offset + bin->tls()->p_filesz)
                    *offset_on_got += tls_.data[tls_.bin_to_index[bin]].file_offset;
                }
                break;
            }

            case R_X86_64_DTPOFF64:
            case R_X86_64_TPOFF64: {
                const std::string name = bin->Str(sym->st_name);
                uintptr_t index = syms_.ResolveCopy(name, soname, version_name);
                newrel.r_info = ELF_R_INFO(index, type);
                LOG(INFO) << ShowRelocationType(type) << " relocation: " << SOLD_LOG_KEY(*rel) << SOLD_LOG_KEY(newrel)
                          << SOLD_LOG_64BITS(bin->OffsetFromAddr(rel->r_offset));
                break;
            }

            case R_X86_64_COPY: {
                const std::string name = bin->Str(sym->st_name);
                uintptr_t index = syms_.ResolveCopy(name, soname, version_name);
                newrel.r_info = ELF_R_INFO(index, type);
                break;
            }

            default:
                LOG(FATAL) << "Unknown relocation type: " << ShowRelocationType(type);
                CHECK(false);
        }

        rels_.push_back(newrel);
    }
}

// Make new relocation table.
// RelocateSymbol_aarch64 rewrites r_offset of each relocation entries
// because we decided locations of shared objects in DecideMemOffset.
void Sold::RelocateSymbol_aarch64(ELFBinary* bin, const Elf_Rel* rel, uintptr_t offset) {
    const Elf_Sym* sym = &bin->symtab()[ELF_R_SYM(rel->r_info)];
    std::string soname, version_name;
    std::tie(soname, version_name) = bin->GetVersion(ELF_R_SYM(rel->r_info), filename_to_soname_);

    int type = ELF_R_TYPE(rel->r_info);
    const uintptr_t addend = rel->r_addend;
    std::vector<Elf_Rel> newrels;

    if (bin->IsVaddrInTLSData(rel->r_offset)) {
        Elf_Rel newrel = *rel;
        const Elf_Phdr* tls = bin->tls();
        CHECK(tls);
        uintptr_t off = newrel.r_offset - tls->p_vaddr;
        off = RemapTLS("reloc", bin, off);
        newrel.r_offset = off + tls_offset_;
        newrels.emplace_back(newrel);
    } else {
        Elf_Rel newrel = *rel;
        newrel.r_offset += offset;
        newrels.emplace_back(newrel);
    }

    if (bin->IsAddrInInitarray(rel->r_offset)) {
        Elf_Rel newrel = *rel;
        CHECK(bin_to_init_array_offset_.find(bin) != bin_to_init_array_offset_.end()) << SOLD_LOG_KEY(bin->filename());

        newrel.r_offset -= bin->init_array_addr();
        newrel.r_offset += bin_to_init_array_offset_[bin];
        LOG(INFO) << SOLD_LOG_BITS(bin->init_array_addr()) << SOLD_LOG_BITS(bin_to_init_array_offset_[bin])
                  << SOLD_LOG_BITS(newrel.r_offset) << SOLD_LOG_BITS(newrel.r_addend) << SOLD_LOG_BITS(offset);
        newrels.emplace_back(newrel);
    } else if (bin->IsAddrInFiniarray(rel->r_offset)) {
        Elf_Rel newrel = *rel;
        CHECK(bin_to_fini_array_offset_.find(bin) != bin_to_fini_array_offset_.end()) << SOLD_LOG_KEY(bin->filename());

        newrel.r_offset -= bin->fini_array_addr();
        newrel.r_offset += bin_to_fini_array_offset_[bin];
        LOG(INFO) << SOLD_LOG_BITS(bin->fini_array_addr()) << SOLD_LOG_BITS(bin_to_fini_array_offset_[bin])
                  << SOLD_LOG_BITS(newrel.r_offset) << SOLD_LOG_BITS(newrel.r_addend) << SOLD_LOG_BITS(offset);
        newrels.emplace_back(newrel);
    }

    LOG(INFO) << "Relocate " << bin->Str(sym->st_name) << " at " << rel->r_offset;

    for (auto newrel : newrels) {
        // Even if we found a defined symbol in src_syms_, we cannot
        // erase the relocation entry. The address needs to be fixed at
        // runtime by ASLR function so we set RELATIVE to these resolved symbols.
        switch (type) {
            case R_AARCH64_RELATIVE: {
                if (IsDefined(*sym)) {
                    LOG(WARNING)
                        << "The symbol associated with R_AARCH64_RELATIVE is defined. Because this relocation type doesn't need any "
                           "symbol, something wrong may have happened.";
                }
                newrel.r_addend += offset;
                break;
            }

            case R_AARCH64_GLOB_DAT:
            case R_AARCH64_JUMP_SLOT: {
                uintptr_t val_or_index;
                if (syms_.Resolve(bin->Str(sym->st_name), soname, version_name, val_or_index)) {
                    newrel.r_info = ELF_R_INFO(0, R_AARCH64_RELATIVE);
                    newrel.r_addend = val_or_index;
                } else {
                    newrel.r_info = ELF_R_INFO(val_or_index, type);
                }
                break;
            }

            case R_AARCH64_ABS64: {
                uintptr_t val_or_index;
                if (syms_.Resolve(bin->Str(sym->st_name), soname, version_name, val_or_index)) {
                    newrel.r_info = ELF_R_INFO(0, R_AARCH64_RELATIVE);
                    newrel.r_addend += val_or_index;
                } else {
                    newrel.r_info = ELF_R_INFO(val_or_index, type);
                }
                break;
            }

            case R_AARCH64_TLSDESC: {
                const std::string name = bin->Str(sym->st_name);
                if (name == "") {
                    LOG(INFO) << SOLD_LOG_KEY(name) << "R_AARCH64_TLSDESC in local dynamic";
                    uintptr_t index = syms_.ResolveCopy(name, soname, version_name);
                    newrel.r_info = ELF_R_INFO(index, type);
                    const bool is_bss = bin->IsOffsetInTLSBSS(newrel.r_addend);
                    if (is_bss) {
                        LOG(INFO) << "R_AARCH64_TLSDESC" << SOLD_LOG_BITS(newrel.r_addend)
                                  << SOLD_LOG_BITS(tls_.data[tls_.bin_to_index[bin]].bss_offset - bin->tls()->p_filesz);
                        newrel.r_addend += tls_.data[tls_.bin_to_index[bin]].bss_offset - bin->tls()->p_filesz;
                    } else {
                        LOG(INFO) << "R_AARCH64_TLSDESC" << SOLD_LOG_BITS(newrel.r_addend)
                                  << SOLD_LOG_BITS(tls_.data[tls_.bin_to_index[bin]].file_offset);
                        newrel.r_addend += tls_.data[tls_.bin_to_index[bin]].file_offset;
                    }
                    break;
                } else {
                    LOG(INFO) << SOLD_LOG_KEY(name) << "R_AARCH64_TLSDESC in generic dynamic";
                    uintptr_t index = syms_.ResolveCopy(name, soname, version_name);
                    newrel.r_info = ELF_R_INFO(index, type);
                    break;
                }
            }

            case R_AARCH64_COPY: {
                const std::string name = bin->Str(sym->st_name);
                uintptr_t index = syms_.ResolveCopy(name, soname, version_name);
                newrel.r_info = ELF_R_INFO(index, type);
                break;
            }

            default:
                LOG(FATAL) << "Unknown relocation type: " << ShowRelocationType(type);
                CHECK(false);
        }

        rels_.push_back(newrel);
    }
}

std::string Sold::ResolveRunPathVariables(const ELFBinary* binary, const std::string& runpath) {
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
        LOG(INFO) << "Unsupported runpath: " << runpath;
        abort();
    }
    return out;
}

std::vector<std::string> Sold::GetLibraryPaths(const ELFBinary* binary) {
    std::vector<std::string> library_paths;
    const std::string& runpath = binary->runpath();
    const std::string& rpath = binary->rpath();
    if (custome_library_path_.empty()) {
        if (runpath.empty() && !rpath.empty()) {
            for (const std::string& path : SplitString(rpath, ":")) {
                library_paths.push_back(ResolveRunPathVariables(binary, path));
            }
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

    if (custome_library_path_.empty()) {
        std::vector<std::string> ldsoconfs = ldsoconf::read_ldsoconf();
        library_paths.insert(library_paths.end(), ldsoconfs.begin(), ldsoconfs.end());

        library_paths.push_back("/lib");
        library_paths.push_back("/usr/lib");
        library_paths.push_back("/usr/lib64");
    }

    library_paths.insert(library_paths.end(), custome_library_path_.begin(), custome_library_path_.end());

    return library_paths;
}

// This implementation is compatible with _dl_sort_maps in glibc/elf/dl-sort-maps.c.
std::vector<ELFBinary*> TopologicalSort(std::vector<std::pair<std::string, ELFBinary*>> link_binaries_buf) {
    if (link_binaries_buf.size() < 1) {
        return {};
    }

    // The third element of tuple is `seen' variable in glibc/elf/dl-sort-maps.c
    std::list<std::tuple<std::string, ELFBinary*, int>> buf;
    for (const auto& p : link_binaries_buf) buf.emplace_back(p.first, p.second, 0);

    auto i_it = buf.begin();
    int n_rest = buf.size();
    while (1) {
        std::get<2>(*i_it)++;

        auto k_it = buf.end();
        k_it--;
        while (i_it != k_it) {
            const auto& neededs = std::get<1>(*k_it)->neededs();
            if (std::find(neededs.begin(), neededs.end(), std::get<0>(*i_it)) != neededs.end()) {
                buf.insert(buf.end(), *i_it);
                i_it = buf.erase(i_it);
                if (std::get<2>(*i_it) > n_rest) break;

                goto next;
            }
            --k_it;
        }
        if (i_it == buf.end() || ++i_it == buf.end()) break;
        for (auto it = i_it; it != buf.end(); it++) std::get<2>(*it) = 0;
    next:;
    }

    std::vector<ELFBinary*> ret;
    for (const auto& t : buf) ret.emplace_back(std::get<1>(t));

    return ret;
}

void Sold::ResolveLibraryPaths(ELFBinary* root_binary) {
    // We should search for shared objects in BFS order.
    std::queue<const ELFBinary*> bfs_queue;
    std::vector<std::pair<std::string, ELFBinary*>> link_binaries_buf;

    bfs_queue.push(root_binary);
    link_binaries_buf.emplace_back("", root_binary);

    while (!bfs_queue.empty()) {
        const ELFBinary* binary = bfs_queue.front();
        bfs_queue.pop();

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
                LOG(FATAL) << "Library " << needed << " not found";
                abort();
            }

            // Register (filename, soname)
            if (library->name() != "" && library->soname() != "") {
                filename_to_soname_[library->name()] = library->soname();
                soname_to_filename_[library->soname()] = library->name();
                LOG(INFO) << SOLD_LOG_KEY(library->name()) << SOLD_LOG_KEY(library->soname());
            } else {
                // soname of shared objects must be non-empty.
                LOG(FATAL) << "Empty filename or soname: " << SOLD_LOG_KEY(library->name()) << SOLD_LOG_KEY(library->soname());
            }

            if (ShouldLink(library->soname(), library->filename())) {
                link_binaries_buf.emplace_back(needed, library.get());
            }

            LOG(INFO) << "Loaded: " << needed << " => " << library->filename();

            auto inserted = libraries_.emplace(needed, std::move(library));
            CHECK(inserted.second);
            bfs_queue.push(inserted.first->second.get());
        }
    }

    link_binaries_ = TopologicalSort(link_binaries_buf);
}

bool Sold::ShouldLink(const std::string& soname, const std::string& filepath) {
    for (const std::string& prefix : EXCLUDE_SHARED_OBJECTS) {
        if (HasPrefix(soname, prefix)) {
            return false;
        }
    }
    for (const std::string& prefix : exclude_sos_) {
        if (HasPrefix(soname, prefix)) {
            return false;
        }
    }
    for (const std::string& prefix : exclude_dirs_) {
        if (HasPrefix(filepath, prefix)) {
            return false;
        }
    }

    return true;
}
