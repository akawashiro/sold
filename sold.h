#include <libgen.h>
#include <sys/stat.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "ehframe_builder.h"
#include "elf_binary.h"
#include "hash.h"
#include "ldsoconf.h"
#include "memprotect_builder.h"
#include "shdr_builder.h"
#include "strtab_builder.h"
#include "symtab_builder.h"
#include "utils.h"
#include "version_builder.h"

class Sold {
public:
    Sold(const std::string& elf_filename, const std::vector<std::string>& exclude_sos, bool emit_section_header);

    void Link(const std::string& out_filename);

    const std::map<std::string, std::string> filename_to_soname() { return filename_to_soname_; };

private:
    void Emit(const std::string& out_filename);

    size_t CountPhdrs() const {
        // DYNAMIC and its LOAD.
        size_t num_phdrs = 2;
        // INTERP and PHDR.
        if (is_executable_) num_phdrs += 2;
        // TLS and its LOAD.
        if (tls_.memsz) num_phdrs += 2;
        // PT_GNU_EH_FRAME and its PT_LOAD
        num_phdrs += 2;
        // GNU_STACK
        num_phdrs++;
        // GNU_RELRO
        num_phdrs++;
        // Normal PT_LOAD
        for (ELFBinary* bin : link_binaries_) {
            num_phdrs += bin->loads().size();
        }
        return num_phdrs;
    }

    uintptr_t GnuHashOffset() const { return sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * CountPhdrs(); }
    uintptr_t GnuHashSize() const { return syms_.GnuHashSize(); }

    uintptr_t SymtabOffset() const { return GnuHashOffset() + GnuHashSize(); }
    uintptr_t SymtabSize() const { return syms_.size() * sizeof(Elf_Sym); }

    uintptr_t VersymOffset() const { return SymtabOffset() + SymtabSize(); }
    uintptr_t VersymSize() const { return version_.SizeVersym(); }

    uintptr_t VerneedOffset() const { return VersymOffset() + VersymSize(); }
    uintptr_t VerneedSize() const { return version_.SizeVerneed(); }

    uintptr_t RelOffset() const { return VerneedOffset() + VerneedSize(); }
    uintptr_t RelSize() const { return rels_.size() * sizeof(Elf_Rel); }

    uintptr_t InitArrayOffset() const { return AlignNext(RelOffset() + RelSize(), 7); }
    uintptr_t InitArraySize() const { return sizeof(uintptr_t) * init_array_.size(); }

    uintptr_t FiniArrayOffset() const { return InitArrayOffset() + InitArraySize(); }
    uintptr_t FiniArraySize() const { return sizeof(uintptr_t) * fini_array_.size(); }

    uintptr_t StrtabOffset() const { return FiniArrayOffset() + FiniArraySize(); }
    uintptr_t StrtabSize() const { return strtab_.size(); }

    uintptr_t DynamicOffset() const { return StrtabOffset() + StrtabSize(); }
    uintptr_t DynamicSize() const { return sizeof(Elf_Dyn) * dynamic_.size(); }

    uintptr_t ShstrtabOffset() const { return DynamicOffset() + DynamicSize(); }
    uintptr_t ShstrtabSize() const { return shdr_.ShstrtabSize(); }

    uintptr_t CodeOffset() const { return AlignNext(ShstrtabOffset() + ShstrtabSize()); }
    uintptr_t CodeSize() {
        uintptr_t p = 0;
        for (const Load& load : loads_) {
            ELFBinary* bin = load.bin;
            Elf_Phdr* phdr = load.orig;
            p += (load.emit.p_offset + phdr->p_filesz);
        }
        return p;
    }

    uintptr_t TLSOffset() const { return tls_file_offset_; }
    uintptr_t TLSFileSize() const {
        static uintptr_t s = 0;
        if (s != 0) return s;
        for (ELFBinary* bin : link_binaries_) {
            for (Elf_Phdr* phdr : bin->phdrs()) {
                if (phdr->p_type == PT_TLS) {
                    s += phdr->p_filesz;
                }
            }
        }
        return s;
    }

    uintptr_t EHFrameOffset() const { return TLSOffset() + TLSFileSize(); }
    // We emit EHFrame whenever the number of FDEs is 0.
    uintptr_t EHFrameSize() const {
        static uintptr_t s = 0;
        if (s != 0) return s;

        int n_fdes = 0;
        s = sizeof(EHFrameHeader::version) + sizeof(EHFrameHeader::eh_frame_ptr_enc) + sizeof(EHFrameHeader::fde_count_enc) +
            sizeof(EHFrameHeader::table_enc) + sizeof(EHFrameHeader::eh_frame_ptr) + sizeof(EHFrameHeader::fde_count);

        for (ELFBinary* bin : link_binaries_) {
            for (Elf_Phdr* phdr : bin->phdrs()) {
                if (phdr->p_type == PT_GNU_EH_FRAME) {
                    n_fdes += bin->eh_frame_header()->fde_count;
                }
            }
        }
        s += n_fdes * (sizeof(EHFrameHeader::FDETableEntry::fde_ptr) + sizeof(EHFrameHeader::FDETableEntry::initial_loc));
        return s;
    }

    uintptr_t MemprotectOffset() const { return memprotect_file_offset_; }
    uintptr_t MemprotectSize() const {
        int n_memprotect = 0;
        for (ELFBinary* bin : link_binaries_) {
            for (Elf_Phdr* phdr : bin->phdrs()) {
                if (phdr->p_type == PT_GNU_RELRO) {
                    n_memprotect++;
                }
            }
        }
        return sizeof(MemprotectBuilder::memprotect_start_code) + sizeof(MemprotectBuilder::memprotect_body_code) * n_memprotect +
               sizeof(MemprotectBuilder::memprotect_end_code);
    }
    uintptr_t ShdrOffset() const { return MemprotectOffset() + MemprotectSize(); }

    void BuildEhdr();

    void BuildLoads();

    void BuildEHFrameHeader() {
        for (const ELFBinary* bin : link_binaries_) {
            for (const Elf_Phdr* phdr : bin->phdrs()) {
                if (phdr->p_type == PT_GNU_EH_FRAME) {
                    // The order of calls of ehframe_builder_.Add is important
                    // because the entries in the table must be sorted by the
                    // initial location value.
                    ehframe_builder_.Add(bin->name(), *bin->eh_frame_header(), bin->AddrFromOffset(phdr->p_offset), offsets_[bin],
                                         ehframe_offset_);
                }
            }
        }
        SOLD_CHECK_EQ(EHFrameSize(), ehframe_builder_.Size());
    }

    void BuildMemprotect() {
        for (const ELFBinary* bin : link_binaries_) {
            const Elf_Phdr* r = bin->gnu_relro();
            if (r) {
                memprotect_builder_.Add(r->p_offset + offsets_[bin], r->p_memsz);
            }
        }
    }

    void MakeDyn(uint64_t tag, uintptr_t ptr) {
        Elf_Dyn dyn;
        dyn.d_tag = tag;
        dyn.d_un.d_ptr = ptr;
        dynamic_.push_back(dyn);
    }

    void BuildInterp() {
        const std::string interp = main_binary_->head() + main_binary_->GetPhdr(PT_INTERP).p_offset;
        LOG(INFO) << "Interp: " << interp;
        interp_offset_ = AddStr(interp);
    }

    void BuildArrays();

    void BuildDynamic();

    void EmitPhdrs(FILE* fp);

    void EmitGnuHash(FILE* fp);

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
            LOG(INFO) << "Emitting code of " << bin->name() << " from " << HexString(ftell(fp)) << " => " << HexString(load.emit.p_offset)
                      << " + " << HexString(phdr->p_filesz);
            EmitPad(fp, load.emit.p_offset);
            WriteBuf(fp, bin->head() + phdr->p_offset, phdr->p_filesz);
        }
    }

    // Emit TLS initialization image
    void EmitTLS(FILE* fp) {
        EmitPad(fp, TLSOffset());
        CHECK(ftell(fp) == TLSOffset());
        for (TLS::Data data : tls_.data) {
            WriteBuf(fp, data.start, data.size);
        }
    }

    void EmitEHFrame(FILE* fp) {
        CHECK(ftell(fp) == EHFrameOffset());
        LOG(INFO) << SOLD_LOG_BITS(ftell(fp)) << SOLD_LOG_BITS(EHFrameOffset()) << SOLD_LOG_BITS(ehframe_builder_.Size());
        ehframe_builder_.Emit(fp);
    }

    void EmitMemprotect(FILE* fp) {
        EmitPad(fp, MemprotectOffset());
        SOLD_CHECK_EQ(ftell(fp), MemprotectOffset());
        LOG(INFO) << SOLD_LOG_BITS(ftell(fp)) << SOLD_LOG_BITS(MemprotectOffset()) << SOLD_LOG_BITS(MemprotectSize());
        memprotect_builder_.Emit(fp, memprotect_offset_);
    }

    void EmitShdr(FILE* fp) {
        SOLD_CHECK_EQ(ftell(fp), ShdrOffset());
        shdr_.EmitShdrs(fp);
    }

    uintptr_t TLSMemSize() const;
    void DecideMemOffset();

    void CollectArrays();

    // CollectSymbols collects all symbols in .dynsym of link_binaries_. When
    // two symbols have the common symbol name, soname, and version, it selects
    // a defined one and throws away the undefined one. If both of the two
    // symbols are defined, one which was found earlier precedes. See
    // LoadDynSymtab for more fine conditions.
    void CollectSymbols() {
        LOG(INFO) << "CollectSymbols";

        std::vector<Syminfo> syms;
        for (ELFBinary* bin : link_binaries_) {
            LoadDynSymtab(bin, syms);
        }
        for (auto s : syms) {
            LOG(INFO) << "SYM " << s.name;
        }
        syms_.SetSrcSyms(syms);
    }

    void CollectTLS();

    void PrintAllVersion() {
        LOG(INFO) << "PrintAllVersion";

        for (ELFBinary* bin : link_binaries_) {
            std::cout << bin->ShowVersion() << std::endl;
        }
    }

    void PrintAllVersyms() {
        LOG(INFO) << "PrintAllVersyms";

        for (ELFBinary* bin : link_binaries_) {
            bin->PrintVersyms();
        }
    }

    uintptr_t RemapTLS(const char* msg, ELFBinary* bin, uintptr_t off);

    void LoadDynSymtab(ELFBinary* bin, std::vector<Syminfo>& symtab);

    void CopyPublicSymbols();

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

    void RelocateSymbol_x86_64(ELFBinary* bin, const Elf_Rel* rel, uintptr_t offset);

    void InitLdLibraryPaths() {
        if (const char* paths = getenv("LD_LIBRARY_PATH")) {
            for (const std::string& path : SplitString(paths, ":")) {
                ld_library_paths_.push_back(path);
            }
        }
    }

    std::string ResolveRunPathVariables(const ELFBinary* binary, const std::string& runpath);

    std::vector<std::string> GetLibraryPaths(const ELFBinary* binary);

    void ResolveLibraryPaths(const ELFBinary* root_binary);

    bool Exists(const std::string& filename) {
        struct stat st;
        if (stat(filename.c_str(), &st) != 0) {
            return false;
        }
        return (st.st_mode & S_IFMT) & S_IFREG;
    }

    bool ShouldLink(const std::string& soname);

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
    std::map<const ELFBinary*, uintptr_t> offsets_;
    std::map<std::string, std::string> filename_to_soname_;
    std::map<std::string, std::string> soname_to_filename_;
    uintptr_t tls_file_offset_{0};
    uintptr_t ehframe_file_offset_{0};
    uintptr_t memprotect_file_offset_{0};
    uintptr_t tls_offset_{0};
    uintptr_t ehframe_offset_{0};
    uintptr_t memprotect_offset_{0};
    bool is_executable_{false};
    bool emit_section_header_;

    uintptr_t interp_offset_;
    SymtabBuilder syms_;
    std::vector<Elf_Rel> rels_;
    StrtabBuilder strtab_;
    VersionBuilder version_;
    EHFrameBuilder ehframe_builder_;
    MemprotectBuilder memprotect_builder_;
    ShdrBuilder shdr_;
    Elf_Ehdr ehdr_;
    std::vector<Load> loads_;
    std::vector<Elf_Dyn> dynamic_;
    std::vector<uintptr_t> init_array_;
    std::vector<uintptr_t> fini_array_;
    TLS tls_;
};
