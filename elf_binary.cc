#include "elf_binary.h"

#include <err.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <numeric>
#include <sstream>

ELFBinary::ELFBinary(const std::string& filename, int fd, char* head, size_t size)
    : filename_(filename), fd_(fd), head_(head), size_(size) {
    LOGF("ELFBinary\n");
    ehdr_ = reinterpret_cast<Elf_Ehdr*>(head);

    {
        size_t found = filename.rfind('/');
        if (found == std::string::npos) {
            name_ = filename;
        } else {
            name_ = filename.substr(found + 1);
        }
    }

    ParsePhdrs();
}

ELFBinary::~ELFBinary() {
    munmap(head_, size_);
    close(fd_);
}

bool ELFBinary::IsELF(const char* p) {
    if (strncmp(p, ELFMAG, SELFMAG)) {
        return false;
    }
    if (p[EI_CLASS] != ELFCLASS64) {
        err(1, "non 64bit ELF isn't supported yet");
    }
    return true;
}

Range ELFBinary::GetRange() const {
    Range range{std::numeric_limits<uintptr_t>::max(), std::numeric_limits<uintptr_t>::min()};
    for (Elf_Phdr* phdr : loads_) {
        range.start = std::min(range.start, phdr->p_vaddr);
        range.end = std::max(range.end, AlignNext(phdr->p_vaddr + phdr->p_memsz));
    }
    return range;
}

bool ELFBinary::InTLS(uintptr_t offset) const {
    if (tls_) {
        return tls_->p_vaddr <= offset && offset < tls_->p_vaddr + tls_->p_memsz;
    }
    return false;
}

void ELFBinary::ReadDynSymtab() {
    CHECK(symtab_);
    LOGF("Read dynsymtab of %s\n", name().c_str());
    if (gnu_hash_) {
        const uint32_t* buckets = gnu_hash_->buckets();
        const uint32_t* hashvals = gnu_hash_->hashvals();
        for (int i = 0; i < gnu_hash_->nbuckets; ++i) {
            int n = buckets[i];
            if (!n) continue;
            const uint32_t* hv = &hashvals[n - gnu_hash_->symndx];
            for (Elf_Sym* sym = &symtab_[n];; ++sym) {
                uint32_t h2 = *hv++;
                const std::string name(strtab_ + sym->st_name);
                // TODO(hamaji): Handle version symbols.

                nsyms_++;
                LOGF("%s@%s index in .dynsymtab = %ld\n", name.c_str(), name_.c_str(), sym - symtab_);
                CHECK(syms_.emplace(std::make_pair(name, sym - symtab_), sym).second);
                if (h2 & 1) break;
            }
        }
        for (size_t n = 0; n < gnu_hash_->symndx; ++n) {
            Elf_Sym* sym = &symtab_[n];
            if (sym->st_name) {
                const std::string name(strtab_ + sym->st_name);

                nsyms_++;
                LOGF("%s@%s index in .dynsymtab = %ld\n", name.c_str(), name_.c_str(), sym - symtab_);
                CHECK(syms_.emplace(std::make_pair(name, sym - symtab_), sym).second);
            }
        }
    } else {
        CHECK(hash_);
        const uint32_t* buckets = hash_->buckets();
        for (size_t i = 0; i < hash_->nbuckets; ++i) {
            int n = buckets[i];
            Elf_Sym* sym = &symtab_[n];
            if (sym->st_name == 0) continue;
            const std::string name(strtab_ + sym->st_name);

            nsyms_++;
            LOGF("%s@%s index in .dynsymtab = %ld\n", name.c_str(), name_.c_str(), sym - symtab_);
            CHECK(syms_.emplace(std::make_pair(name, sym - symtab_), sym).second);
        }
    }
    LOGF("nsyms_ = %d\n", nsyms_);
}

Elf_Phdr* ELFBinary::FindPhdr(uint64_t type) {
    for (Elf_Phdr* phdr : phdrs_) {
        if (phdr->p_type == type) {
            return phdr;
        }
    }
    return nullptr;
}

const Elf_Phdr& ELFBinary::GetPhdr(uint64_t type) {
    Elf_Phdr* phdr = FindPhdr(type);
    CHECK(phdr);
    return *phdr;
}

void ELFBinary::ParseVersyms() {
    CHECK(versym_);
    CHECK(nsyms_);

    for (int i = 0; i < nsyms_ + 1; i++) {
        if (versym_[i] == VER_NDX_LOCAL) {
            LOGF("VERSYM: VER_NDX_LOCAL\n");
        } else if (versym_[i] == VER_NDX_GLOBAL) {
            LOGF("VERSYM: VER_NDX_GLOBAL\n");
        } else {
            LOGF("VERSYM: %d\n", versym_[i]);
        }
    }
}

void ELFBinary::ParseVerneeds() {
    CHECK(verneed_);
    Elf_Verneed* vn = verneed_;
    for (int i = 0; i < verneednum_; ++i) {
        LOGF("VERNEED: ver=%d cnt=%d file=%s aux=%d next=%d\n", vn->vn_version, vn->vn_cnt, strtab_ + vn->vn_file, vn->vn_aux, vn->vn_next);
        Elf_Vernaux* vna = (Elf_Vernaux*)((char*)vn + vn->vn_aux);
        for (int j = 0; j < vn->vn_cnt; ++j) {
            LOGF(" VERNAUX: hash=%d flags=%d other=%d name=%s next=%d\n", vna->vna_hash, vna->vna_flags, vna->vna_other,
                 strtab_ + vna->vna_name, vna->vna_next);

            // int ver = vna->vna_other;
            // if (num_verstrs <= ver) {
            //     num_verstrs = ver + 1;
            //     obj->verstrs = (const char**)realloc(obj->verstrs, sizeof(char*) * num_verstrs);
            // }
            // obj->verstrs[ver] = strtab_ + vna->vna_name;

            vna = (Elf_Vernaux*)((char*)vna + vna->vna_next);
        }
        vn = (Elf_Verneed*)((char*)vn + vn->vn_next);
    }
}

std::string ELFBinary::ShowVersym(int index) {
    CHECK(0 < index && index <= nsyms_ + 1);
    if (versym_[index] == VER_NDX_LOCAL) {
        return std::string("VER_NDX_LOCAL");
    } else if (versym_[index] == VER_NDX_GLOBAL) {
        return std::string("VER_NDX_GLOBAL");
    } else {
        CHECK(verneed_);
        Elf_Verneed* vn = verneed_;
        for (int i = 0; i < verneednum_; ++i) {
            LOGF("VERNEED: ver=%d cnt=%d file=%s aux=%d next=%d\n", vn->vn_version, vn->vn_cnt, strtab_ + vn->vn_file, vn->vn_aux,
                 vn->vn_next);
            Elf_Vernaux* vna = (Elf_Vernaux*)((char*)vn + vn->vn_aux);
            for (int j = 0; j < vn->vn_cnt; ++j) {
                LOGF(" VERNAUX: hash=%d flags=%d other=%d name=%s next=%d\n", vna->vna_hash, vna->vna_flags, vna->vna_other,
                     strtab_ + vna->vna_name, vna->vna_next);

                if (vna->vna_other == versym_[index]) {
                    std::stringstream ss;
                    ss << std::string(strtab_ + vna->vna_name) << " (" << versym_[index] << ")";
                    return ss.str();
                }

                vna = (Elf_Vernaux*)((char*)vna + vna->vna_next);
            }
            vn = (Elf_Verneed*)((char*)vn + vn->vn_next);
        }
        LOGF("Failed to find Elf_Vernaux corresponds to %d\n", versym_[index]);
        exit(1);
    }
}

void ELFBinary::ParsePhdrs() {
    LOGF("ParsePhdrs\n");

    for (int i = 0; i < ehdr_->e_phnum; ++i) {
        Elf_Phdr* phdr = reinterpret_cast<Elf_Phdr*>(head_ + ehdr_->e_phoff + ehdr_->e_phentsize * i);
        phdrs_.push_back(phdr);
        if (phdr->p_type == PT_LOAD) {
            loads_.push_back(phdr);
        } else if (phdr->p_type == PT_TLS) {
            tls_ = phdr;
        }
    }

    for (Elf_Phdr* phdr : phdrs_) {
        if (phdr->p_type == PT_DYNAMIC) {
            ParseDynamic(phdr->p_offset, phdr->p_filesz);
        } else if (phdr->p_type == PT_INTERP) {
            LOGF("Found PT_INTERP.\n");
        }
    }
    CHECK(!phdrs_.empty());
}

void ELFBinary::ParseDynamic(size_t off, size_t size) {
    size_t dyn_size = sizeof(Elf_Dyn);
    CHECK(size % dyn_size == 0);
    std::vector<Elf_Dyn*> dyns;
    for (size_t i = 0; i < size / dyn_size; ++i) {
        Elf_Dyn* dyn = reinterpret_cast<Elf_Dyn*>(head_ + off + dyn_size * i);
        dyns.push_back(dyn);
    }

    uintptr_t* init_array{0};
    uintptr_t init_arraysz{0};
    uintptr_t* fini_array{0};
    uintptr_t fini_arraysz{0};
    for (Elf_Dyn* dyn : dyns) {
        auto get_ptr = [this, dyn]() { return GetPtr(dyn->d_un.d_ptr); };
        if (dyn->d_tag == DT_STRTAB) {
            strtab_ = get_ptr();
        } else if (dyn->d_tag == DT_SYMTAB) {
            symtab_ = reinterpret_cast<Elf_Sym*>(get_ptr());
        } else if (dyn->d_tag == DT_GNU_HASH) {
            gnu_hash_ = reinterpret_cast<Elf_GnuHash*>(get_ptr());
        } else if (dyn->d_tag == DT_HASH) {
            hash_ = reinterpret_cast<Elf_Hash*>(get_ptr());
        } else if (dyn->d_tag == DT_RELA) {
            rel_ = reinterpret_cast<Elf_Rel*>(get_ptr());
        } else if (dyn->d_tag == DT_RELASZ) {
            num_rels_ = dyn->d_un.d_val / sizeof(Elf_Rel);
        } else if (dyn->d_tag == DT_JMPREL) {
            plt_rel_ = reinterpret_cast<Elf_Rel*>(get_ptr());
        } else if (dyn->d_tag == DT_PLTRELSZ) {
            num_plt_rels_ = dyn->d_un.d_val / sizeof(Elf_Rel);
        } else if (dyn->d_tag == DT_PLTREL) {
            CHECK(dyn->d_un.d_val == DT_RELA);
        } else if (dyn->d_tag == DT_PLTREL) {
            // TODO(hamaji): Check
        } else if (dyn->d_tag == DT_REL || dyn->d_tag == DT_RELSZ || dyn->d_tag == DT_RELENT) {
            // TODO(hamaji): Support 32bit?
            CHECK(false);
        } else if (dyn->d_tag == DT_INIT_ARRAY) {
            init_array = reinterpret_cast<uintptr_t*>(get_ptr());
        } else if (dyn->d_tag == DT_INIT_ARRAYSZ) {
            init_arraysz = dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_FINI_ARRAY) {
            fini_array = reinterpret_cast<uintptr_t*>(get_ptr());
        } else if (dyn->d_tag == DT_FINI_ARRAYSZ) {
            fini_arraysz = dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_INIT) {
            init_ = dyn->d_un.d_ptr;
        } else if (dyn->d_tag == DT_FINI) {
            fini_ = dyn->d_un.d_ptr;
        } else if (dyn->d_tag == DT_VERSYM) {
            LOGF("Found versym.\n");
            versym_ = reinterpret_cast<Elf_Versym*>(get_ptr());
        } else if (dyn->d_tag == DT_VERNEED) {
            LOGF("Found verneed\n");
            verneed_ = reinterpret_cast<Elf_Verneed*>(get_ptr());
        } else if (dyn->d_tag == DT_VERNEEDNUM) {
            verneednum_ = dyn->d_un.d_val;
            LOGF("Found verneednum %ld\n", verneednum_);
        }
    }
    CHECK(strtab_);

    ParseFuncArray(init_array, init_arraysz, &init_array_);
    ParseFuncArray(fini_array, fini_arraysz, &fini_array_);

    for (Elf_Dyn* dyn : dyns) {
        if (dyn->d_tag == DT_NEEDED) {
            const char* needed = strtab_ + dyn->d_un.d_val;
            neededs_.push_back(needed);
        } else if (dyn->d_tag == DT_SONAME) {
            name_ = soname_ = strtab_ + dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_RUNPATH) {
            runpath_ = strtab_ + dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_RPATH) {
            rpath_ = strtab_ + dyn->d_un.d_val;
        }
    }
}

void ELFBinary::ParseFuncArray(uintptr_t* array, uintptr_t size, std::vector<uintptr_t>* out) {
    for (size_t i = 0; i < size / sizeof(uintptr_t); ++i) {
        out->push_back(array[i]);
    }
}

Elf_Addr ELFBinary::OffsetFromAddr(Elf_Addr addr) {
    for (Elf_Phdr* phdr : loads_) {
        if (phdr->p_vaddr <= addr && addr < phdr->p_vaddr + phdr->p_memsz) {
            return addr - phdr->p_vaddr + phdr->p_offset;
        }
    }
    LOGF("Address %llx cannot be resolved\n", static_cast<long long>(addr));
    abort();
}

std::string ELFBinary::ShowDynSymtab() {
    LOGF("ShowDynSymtab\n");
    std::vector<std::string> res(syms_.size() + 1);
    for (auto it : syms_) {
        std::stringstream ss;
        ss << it.first.second << ": " << it.first.first << " ";

        if (versym_) {
            ss << ShowVersym(it.first.second) << "\n";
        } else {
            ss << "NO_VERSION_INFO\n";
        }
        res[it.first.second] = ss.str();
    }

    return std::accumulate(res.begin(), res.end(), std::string(""));
}

std::unique_ptr<ELFBinary> ReadELF(const std::string& filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) err(1, "open failed: %s", filename.c_str());

    size_t size = lseek(fd, 0, SEEK_END);
    if (size < 8 + 16) err(1, "too small file: %s", filename.c_str());

    size_t mapped_size = (size + 0xfff) & ~0xfff;

    char* p = (char*)mmap(NULL, mapped_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) err(1, "mmap failed: %s", filename.c_str());

    if (ELFBinary::IsELF(p)) {
        return std::make_unique<ELFBinary>(filename.c_str(), fd, p, mapped_size);
    }
    err(1, "unknown file format: %s", filename.c_str());
}
