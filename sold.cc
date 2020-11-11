#include "sold.h"

#include <queue>
#include <set>

Sold::Sold(const std::string& elf_filename, const std::vector<std::string>& exclude_sos, bool emit_section_header)
    : exclude_sos_(exclude_sos), emit_section_header_(emit_section_header) {
    main_binary_ = ReadELF(elf_filename);
    is_executable_ = main_binary_->FindPhdr(PT_INTERP);
    link_binaries_.push_back(main_binary_.get());

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
    DecideOffsets();
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

    strtab_.Freeze();
    BuildLoads();

    shdr_.RegisterShdr(GnuHashOffset(), GnuHashSize(), ShdrBuilder::ShdrType::GnuHash);
    shdr_.RegisterShdr(SymtabOffset(), SymtabSize(), ShdrBuilder::ShdrType::Dynsym, sizeof(Elf_Sym));
    shdr_.RegisterShdr(VersymOffset(), VersymSize(), ShdrBuilder::ShdrType::GnuVersion, sizeof(Elf_Versym));
    shdr_.RegisterShdr(VerneedOffset(), VerneedSize(), ShdrBuilder::ShdrType::GnuVersionR, 0, version_.NumVerneed());
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
}

void Sold::BuildArrays() {
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
    if (!main_binary_->rpath().empty()) {
        MakeDyn(DT_RPATH, AddStr(main_binary_->rpath()));
    }
    if (!main_binary_->runpath().empty()) {
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

// Decide locations for each linked shared objects
// TODO(akawashiro) Is the initial value of offset optimal?
void Sold::DecideOffsets() {
    uintptr_t offset = 0x10000000;
    for (ELFBinary* bin : link_binaries_) {
        const Range range = bin->GetRange() + offset;
        CHECK(range.start == offset);
        offsets_.emplace(bin, range.start);
        LOG(INFO) << "Assigned: " << bin->soname() << " " << HexString(range.start, 8) << "-" << HexString(range.end, 8);
        offset = range.end;
    }
    tls_offset_ = offset;
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

    for (TLS::Data& d : tls_.data) {
        d.bss_offset += tls_.filesz;
        LOG(INFO) << "TLS of " << d.bin->name() << ": file=" << HexString(d.file_offset) << " + " << HexString(d.size)
                  << " mem=" << HexString(d.bss_offset);
    }

    LOG(INFO) << "TLS: filesz=" << HexString(tls_.filesz) << " memsz=" << HexString(tls_.memsz) << " cnt=" << HexString(tls_.data.size());
}

// Collect .init_array and .fini_array
void Sold::CollectArrays() {
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
        LOG(INFO) << "Symbol " << name << "@" << bin->name() << " " << sym->st_value;

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
        if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL && IsDefined(*sym)) {
            LOG(INFO) << "Copy public symbol " << p.name;
            CHECK(is_special_ver_ndx(p.versym) || p.versym == VersionBuilder::NEED_NEW_VERNUM) << SOLD_LOG_KEY(p);
            syms_.AddPublicSymbol(p);
        }
    }
    for (ELFBinary* bin : link_binaries_) {
        if (bin == main_binary_.get()) continue;
        for (const auto& p : bin->GetSymbolMap()) {
            const Elf_Sym* sym = p.sym;
            if (IsTLS(*sym)) {
                LOG(INFO) << "Copy TLS symbol " << p.name;
                CHECK(is_special_ver_ndx(p.versym) || p.versym == VersionBuilder::NEED_NEW_VERNUM) << SOLD_LOG_KEY(p);
                syms_.AddPublicSymbol(p);
            }
        }
    }
}

// Make new relocation table.
// RelocateSymbol_x86_64 rewrites r_offset of each relocation entries
// because we decided locations of shared objects in DecideOffsets.
void Sold::RelocateSymbol_x86_64(ELFBinary* bin, const Elf_Rel* rel, uintptr_t offset) {
    const Elf_Sym* sym = &bin->symtab()[ELF_R_SYM(rel->r_info)];
    auto [soname, version_name] = bin->GetVersion(ELF_R_SYM(rel->r_info), filename_to_soname_);

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

    LOG(INFO) << "Relocate " << bin->Str(sym->st_name) << " at " << rel->r_offset;

    // Even if we found a defined symbol in src_syms_, we cannot
    // erase the relocation entry. The address needs to be fixed at
    // runtime by ASLR function so we set RELATIVE to these resolved symbols.
    switch (type) {
        case R_X86_64_RELATIVE: {
            if (IsDefined(*sym)) {
                LOG(WARNING) << "The symbol associated with R_X86_64_RELATIVE is defined. Because this relocation type doesn't need any "
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

        // TODO(akawashiro) Fill here with some exaplanation.
        case R_X86_64_DTPMOD64:
        case R_X86_64_DTPOFF64:
        case R_X86_64_COPY: {
            const std::string name = bin->Str(sym->st_name);
            uintptr_t index = syms_.ResolveCopy(name, soname, version_name);
            newrel.r_info = ELF_R_INFO(index, type);
            break;
        }

        default:
            LOG(FATAL) << "Unknown relocation type: " << type;
            CHECK(false);
    }

    rels_.push_back(newrel);
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

void Sold::ResolveLibraryPaths(const ELFBinary* root_binary) {
    // We should search for shared objects in BFS order.
    std::queue<const ELFBinary*> bfs_queue;
    std::set<std::string> pushed_sonames;

    bfs_queue.push(root_binary);

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

            if (pushed_sonames.find(library->soname()) != pushed_sonames.end()) {
                LOG(INFO) << SOLD_LOG_KEY(library->name())
                          << " is not linked because we have already linked another shared object with the same soname.";
                continue;
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

            if (ShouldLink(library->soname())) {
                link_binaries_.push_back(library.get());
            }

            LOG(INFO) << "Loaded: " << needed << " => " << library->filename();

            auto inserted = libraries_.emplace(needed, std::move(library));
            CHECK(inserted.second);
            bfs_queue.push(inserted.first->second.get());
        }
    }
}

bool Sold::ShouldLink(const std::string& soname) {
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
