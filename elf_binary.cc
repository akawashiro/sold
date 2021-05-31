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

#include "elf_binary.h"

#include <err.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <numeric>
#include <set>
#include <sstream>

#include "version_builder.h"

ELFBinary::ELFBinary(const std::string& filename, int fd, char* head, size_t size)
    : filename_(filename), fd_(fd), head_(head), size_(size) {
    ehdr_ = reinterpret_cast<Elf_Ehdr*>(head);

    CHECK_EQ(ehdr_->e_type, ET_DYN);
    CHECK((ehdr_->e_machine, EM_X86_64) || (ehdr_->e_machine, EM_AARCH64));

    {
        size_t found = filename.rfind('/');
        if (found == std::string::npos) {
            name_ = filename;
        } else {
            name_ = filename.substr(found + 1);
        }
        LOG(INFO) << SOLD_LOG_KEY(name_);
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

bool ELFBinary::IsVaddrInTLSData(uintptr_t vaddr) const {
    if (tls_) {
        return tls_->p_vaddr <= vaddr && vaddr < (tls_->p_vaddr + tls_->p_filesz);
    }
    return false;
}

bool ELFBinary::IsOffsetInTLSData(uintptr_t tls_offset) const {
    if (tls_ && tls_offset < tls_->p_memsz) {
        return tls_offset < tls_->p_filesz;
    }
    LOG(FATAL) << SOLD_LOG_KEY(tls_) << SOLD_LOG_KEY(tls_offset);
}

bool ELFBinary::IsOffsetInTLSBSS(uintptr_t tls_offset) const {
    if (tls_ && tls_offset < tls_->p_memsz) {
        return tls_->p_filesz <= tls_offset;
    }
    LOG(FATAL) << SOLD_LOG_KEY(tls_) << SOLD_LOG_KEY(tls_offset);
}

namespace {

std::set<int> CollectSymbolsFromReloc(const Elf_Rel* rels, size_t num) {
    std::set<int> indices;
    for (size_t i = 0; i < num; ++i) {
        const Elf_Rel* rel = &rels[i];
        indices.insert(ELF_R_SYM(rel->r_info));
    }
    return indices;
}

std::set<int> CollectSymbolsFromGnuHash(Elf_GnuHash* gnu_hash) {
    std::set<int> indices;
    const uint32_t* buckets = gnu_hash->buckets();
    const uint32_t* hashvals = gnu_hash->hashvals();
    for (int i = 0; i < gnu_hash->nbuckets; ++i) {
        int n = buckets[i];
        if (!n) continue;
        const uint32_t* hv = &hashvals[n - gnu_hash->symndx];
        for (;; ++n) {
            uint32_t h2 = *hv++;
            CHECK(indices.insert(n).second);
            if (h2 & 1) break;
        }
    }
    for (size_t n = 0; n < gnu_hash->symndx; ++n) {
        indices.insert(n);
    }
    return indices;
}

std::set<int> CollectSymbolsFromElfHash(const std::string& name, Elf_Hash* hash) {
    std::set<int> indices;
    const uint32_t* buckets = hash->buckets();
    const uint32_t* chains = hash->chains();
    for (size_t i = 0; i < hash->nbuckets; ++i) {
        for (int n = buckets[i]; n != STN_UNDEF; n = chains[n]) {
            indices.insert(n);
        }
    }
    return indices;
}

}  // namespace

void ELFBinary::ReadDynSymtab(const std::map<std::string, std::string>& filename_to_soname) {
    CHECK(symtab_);
    LOG(INFO) << "Read dynsymtab of " << name();

    // Since we only rely on program headers and do not read section headers
    // at all, we do not know the exact size of .dynsym section. We collect
    // indices in .dynsym from both (GNU or ELF) hash and relocs.

    std::set<int> indices;
    if (gnu_hash_) {
        indices = CollectSymbolsFromGnuHash(gnu_hash_);
    } else {
        CHECK(hash_);
        indices = CollectSymbolsFromElfHash(name(), hash_);
    }

    for (int idx : CollectSymbolsFromReloc(rel_, num_rels_)) indices.insert(idx);
    for (int idx : CollectSymbolsFromReloc(plt_rel_, num_plt_rels_)) indices.insert(idx);

    std::set<std::tuple<std::string, std::string, std::string>> duplicate_check;

    for (int idx : indices) {
        Elf_Sym* sym = &symtab_[idx];
        if (sym->st_name == 0) continue;
        const std::string symname(strtab_ + sym->st_name);

        nsyms_++;
        LOG(INFO) << symname << "@" << name() << " index in .dynsym = " << idx;

        // Get version information coresspoinds to idx
        std::string soname, version;
        std::tie(soname, version) = GetVersion(idx, filename_to_soname);
        Elf_Versym v = versym_ ? versym_[idx] : NO_VERSION_INFO;

        syms_.push_back(Syminfo{symname, soname, version, v, sym});
        CHECK(duplicate_check.insert({symname, soname, version}).second)
            << SOLD_LOG_KEY(symname) << SOLD_LOG_KEY(soname) << SOLD_LOG_KEY(version);
        LOG(INFO) << "duplicate_check: " << SOLD_LOG_KEY(symname) << SOLD_LOG_KEY(version);
    }

    LOG(INFO) << "nsyms_ = " << nsyms_;
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

// GetVersion returns (soname, version)
std::pair<std::string, std::string> ELFBinary::GetVersion(int index, const std::map<std::string, std::string>& filename_to_soname) {
    LOG(INFO) << "GetVersion";
    if (!versym_) {
        return std::make_pair("", "");
    }

    LOG(INFO) << SOLD_LOG_KEY(versym_[index]);

    if (is_special_ver_ndx(versym_[index])) {
        return std::make_pair("", "");
    } else {
        if (verneed_) {
            Elf_Verneed* vn = verneed_;
            for (int i = 0; i < verneednum_; ++i) {
                LOG(INFO) << "Elf_Verneed: " << SOLD_LOG_KEY(vn->vn_version) << SOLD_LOG_KEY(vn->vn_cnt)
                          << SOLD_LOG_KEY(strtab_ + vn->vn_file) << SOLD_LOG_KEY(vn->vn_aux) << SOLD_LOG_KEY(vn->vn_next);
                Elf_Vernaux* vna = (Elf_Vernaux*)((char*)vn + vn->vn_aux);
                for (int j = 0; j < vn->vn_cnt; ++j) {
                    LOG(INFO) << "Elf_Vernaux: " << SOLD_LOG_KEY(vna->vna_hash) << SOLD_LOG_KEY(vna->vna_flags)
                              << SOLD_LOG_KEY(vna->vna_other) << SOLD_LOG_KEY(strtab_ + vna->vna_name) << SOLD_LOG_KEY(vna->vna_next);

                    if (vna->vna_other == versym_[index]) {
                        LOG(INFO) << "Find Elf_Vernaux corresponds to " << versym_[index] << SOLD_LOG_KEY(strtab_ + vn->vn_file)
                                  << SOLD_LOG_KEY(strtab_ + vna->vna_name);

                        std::string filename = std::string(strtab_ + vn->vn_file);
                        auto found = filename_to_soname.find(filename);
                        if (found != filename_to_soname.end()) {
                            return std::make_pair(found->second, std::string(strtab_ + vna->vna_name));
                        } else {
                            LOG(FATAL) << "There is no entry for " << filename << " in filename_to_soname.";
                        }
                    }

                    vna = (Elf_Vernaux*)((char*)vna + vna->vna_next);
                }
                vn = (Elf_Verneed*)((char*)vn + vn->vn_next);
            }
        }
        if (verdef_) {
            Elf_Verdef* vd = verdef_;
            std::string soname, version;
            for (int i = 0; i < verdefnum_; ++i) {
                Elf_Verdaux* vda = (Elf_Verdaux*)((char*)vd + vd->vd_aux);

                if (vd->vd_flags & VER_FLG_BASE) {
                    soname = std::string(strtab_ + vda->vda_name);
                }
                if (vd->vd_ndx == versym_[index]) {
                    version = std::string(strtab_ + vda->vda_name);
                }

                vd = (Elf_Verdef*)((char*)vd + vd->vd_next);
            }
            if (soname != "" && version != "") {
                LOG(INFO) << "Find Elf_Verdef corresponds to " << versym_[index] << SOLD_LOG_KEY(soname) << SOLD_LOG_KEY(version);
                return std::make_pair(soname, version);
            }
        }

        LOG(WARNING) << "Find no entry corresponds to " << versym_[index];
        return std::make_pair("", "");
    }
}

void ELFBinary::PrintVersyms() {
    if (!versym_ || !nsyms_) return;

    for (int i = 0; i < nsyms_ + 1; i++) {
        if (is_special_ver_ndx(versym_[i])) {
            LOG(INFO) << "VERSYM: " << special_ver_ndx_to_str(versym_[i]);
        } else {
            LOG(INFO) << "VERSYM: " << versym_[i];
        }
    }
}

std::string ELFBinary::ShowVersion() {
    std::stringstream ss;
    if (verneed_) {
        Elf_Verneed* vn = verneed_;
        for (int i = 0; i < verneednum_; ++i) {
            ss << "VERNEED: ver=" << vn->vn_version << " cnt=" << vn->vn_cnt << " file=" << strtab_ + vn->vn_file << " aux=" << vn->vn_aux
               << " next=" << vn->vn_next << "\n";
            Elf_Vernaux* vna = (Elf_Vernaux*)((char*)vn + vn->vn_aux);
            for (int j = 0; j < vn->vn_cnt; ++j) {
                ss << " VERNAUX: hash=" << vna->vna_hash << " flags=" << vna->vna_flags << " other=" << vna->vna_other
                   << " name=" << strtab_ + vna->vna_name << " next=" << vna->vna_next << "\n";

                vna = (Elf_Vernaux*)((char*)vna + vna->vna_next);
            }
            vn = (Elf_Verneed*)((char*)vn + vn->vn_next);
        }
    }
    if (verdef_) {
        Elf_Verdef* vd = verdef_;
        for (int i = 0; i < verdefnum_; ++i) {
            ss << "VERDEF: flags=" << vd->vd_flags << " ndx=" << vd->vd_ndx << " cnt=" << vd->vd_cnt << " hash=" << vd->vd_hash
               << " next=" << vd->vd_next << std::endl;
            Elf_Verdaux* vda = (Elf_Verdaux*)((char*)vd + vd->vd_aux);
            for (int j = 0; j < vd->vd_cnt; ++j) {
                ss << "    VERDEF: name=" << strtab_ + vda->vda_name << " next=" << vda->vda_next << std::endl;
                vda = (Elf_Verdaux*)((char*)vda + vda->vda_next);
            }
            vd = (Elf_Verdef*)((char*)vd + vd->vd_next);
        }
    }
    return ss.str();
}

void ELFBinary::ParsePhdrs() {
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
            LOG(INFO) << "Found PT_INTERP.";
        } else if (phdr->p_type == PT_GNU_EH_FRAME) {
            ParseEHFrameHeader(phdr->p_offset, phdr->p_filesz);
        } else if (phdr->p_type == PT_GNU_STACK) {
            gnu_stack_ = phdr;
        } else if (phdr->p_type == PT_GNU_RELRO) {
            gnu_relro_ = phdr;
        }
    }
    CHECK(!phdrs_.empty());
}

void ELFBinary::ParseEHFrameHeader(size_t off, size_t size) {
    const char* const efh_base = head_ + off;
    int efh_offset = 0;
    auto efh_read = [efh_base, &efh_offset](auto* p) {
        memcpy(p, efh_base + efh_offset, sizeof(*p));
        efh_offset += sizeof(*p);
    };

    efh_read(&eh_frame_header_.version);
    efh_read(&eh_frame_header_.eh_frame_ptr_enc);
    efh_read(&eh_frame_header_.fde_count_enc);
    efh_read(&eh_frame_header_.table_enc);

    CHECK(eh_frame_header_.version == 1);
    CHECK(eh_frame_header_.eh_frame_ptr_enc == (DW_EH_PE_sdata4 | DW_EH_PE_pcrel));
    CHECK(eh_frame_header_.fde_count_enc == DW_EH_PE_udata4);
    CHECK(eh_frame_header_.table_enc == (DW_EH_PE_sdata4 | DW_EH_PE_datarel));

    efh_read(&eh_frame_header_.eh_frame_ptr);
    efh_read(&eh_frame_header_.fde_count);

    LOG(INFO) << "ParseEHFrameHeader" << SOLD_LOG_KEY(off) << SOLD_LOG_KEY(size) << SOLD_LOG_8BITS(eh_frame_header_.version)
              << SOLD_LOG_DWEHPE(eh_frame_header_.eh_frame_ptr_enc) << SOLD_LOG_DWEHPE(eh_frame_header_.fde_count_enc)
              << SOLD_LOG_DWEHPE(eh_frame_header_.table_enc) << SOLD_LOG_32BITS(eh_frame_header_.eh_frame_ptr)
              << SOLD_LOG_KEY(eh_frame_header_.fde_count);

    CHECK(efh_offset + eh_frame_header_.fde_count * (sizeof(int32_t) * 2) <= size)
        << SOLD_LOG_KEY(efh_offset + eh_frame_header_.fde_count * (sizeof(int32_t) * 2)) << SOLD_LOG_KEY(size);

    for (int i = 0; i < eh_frame_header_.fde_count; i++) {
        EHFrameHeader::FDETableEntry e;
        efh_read(&e.initial_loc);
        efh_read(&e.fde_ptr);
        eh_frame_header_.table.emplace_back(e);

        LOG(INFO) << SOLD_LOG_32BITS(e.initial_loc) << SOLD_LOG_32BITS(e.fde_ptr) << SOLD_LOG_32BITS(off + e.fde_ptr)
                  << SOLD_LOG_32BITS(AddrFromOffset(off)) << SOLD_LOG_32BITS(AddrFromOffset(off) + e.fde_ptr)
                  << SOLD_LOG_32BITS(OffsetFromAddr(AddrFromOffset(off) + e.fde_ptr));

        EHFrameHeader::FDE fde = {};
        EHFrameHeader::CIE cie = {};
        cie.FDE_encoding = DW_EH_PE_SOLD_DUMMY;
        cie.LSDA_encoding = DW_EH_PE_SOLD_DUMMY;

        const char* const fde_base = head_ + OffsetFromAddr(AddrFromOffset(off) + e.fde_ptr);
        int fde_offset = 0;
        auto fde_read = [fde_base, &fde_offset](auto* p) {
            memcpy(p, fde_base + fde_offset, sizeof(*p));
            fde_offset += sizeof(*p);
        };

        fde_read(&fde.length);
        if (fde.length == 0xffffffff) {
            fde_read(&fde.extended_length);
        }
        fde_read(&fde.CIE_delta);

        // fde_base + fde_offset - sizeof(int32_t) is the address of fde.CIE_delta.
        const char* const cie_base =
            head_ + OffsetFromAddr(AddrFromOffset(fde_base + fde_offset - sizeof(int32_t) - head_) - fde.CIE_delta);
        int cie_offset = 0;
        auto cie_read = [cie_base, &cie_offset](auto* p) {
            memcpy(p, cie_base + cie_offset, sizeof(*p));
            cie_offset += sizeof(*p);
        };
        uint32_t utmp;
        int32_t stmp;

        cie_read(&cie.length);
        cie_read(&cie.CIE_id);
        cie_read(&cie.version);
        cie.aug_str = cie_base + cie_offset;
        while (*(cie_base + cie_offset) != '\0') cie_offset++;
        cie_offset++;
        cie_offset = read_uleb128(reinterpret_cast<const char*>(cie_base + cie_offset), &utmp) - cie_base;  // Skip code alignment factor
        cie_offset = read_sleb128(reinterpret_cast<const char*>(cie_base + cie_offset), &stmp) - cie_base;  // Skip data alignment factor
        cie_offset = read_uleb128(reinterpret_cast<const char*>(cie_base + cie_offset), &utmp) - cie_base;  // Skip augmentation factor

        const char* aug_head = cie.aug_str;
        if (*aug_head == 'z') {
            aug_head++;
            cie_offset++;

            LOG(INFO) << SOLD_LOG_8BITS(*(cie_base + cie_offset)) << SOLD_LOG_KEY(*aug_head);

            // Copy from sysdeps/generic/unwind-dw2-fde.c in glibc
            while (1) {
                if (*aug_head == 'R') {
                    cie_read(&cie.FDE_encoding);
                } else if (*aug_head == 'P') {
                    /* Personality encoding and pointer.  */
                    /* ??? Avoid dereferencing indirect pointers, since we're
                       faking the base address.  Gotta keep DW_EH_PE_aligned
                       intact, however.  */
                    cie_offset =
                        read_encoded_value_with_base(*(cie_base + cie_offset) & 0x7F, 0, cie_base + cie_offset + 1, &utmp) - cie_base;
                } else if (*aug_head == 'L') {
                    cie_read(&cie.LSDA_encoding);
                } else {
                    if (*aug_head != '\0') {
                        LOG(WARNING) << "unknown augmentation" << SOLD_LOG_KEY(*aug_head) << SOLD_LOG_8BITS(*aug_head);
                    }
                    break;
                }
                aug_head++;
            }
        }

        fde_read(&fde.initial_loc);

        LOG(INFO) << "ParseEHFrameHeader table[" << i << "] = {" << SOLD_LOG_32BITS(e.initial_loc) << SOLD_LOG_32BITS(e.fde_ptr)
                  << "} FDE = {" << SOLD_LOG_32BITS(fde.length) << SOLD_LOG_64BITS(fde.extended_length) << SOLD_LOG_32BITS(fde.CIE_delta)
                  << SOLD_LOG_32BITS(fde.initial_loc) << "} CIE = {" << SOLD_LOG_32BITS(cie.length) << SOLD_LOG_32BITS(cie.CIE_id)
                  << SOLD_LOG_8BITS(cie.version) << SOLD_LOG_KEY(cie.aug_str) << SOLD_LOG_DWEHPE(cie.FDE_encoding)
                  << SOLD_LOG_DWEHPE(cie.LSDA_encoding) << "}";

        CHECK(cie.FDE_encoding == (DW_EH_PE_sdata4 | DW_EH_PE_pcrel));
        CHECK(cie.LSDA_encoding == (DW_EH_PE_sdata4 | DW_EH_PE_pcrel) || cie.LSDA_encoding == DW_EH_PE_SOLD_DUMMY);

        eh_frame_header_.fdes.emplace_back(fde);
        eh_frame_header_.cies.emplace_back(cie);
    }
}

std::string ELFBinary::ShowEHFrame() {
    std::stringstream ss;

    ss << "version: " << HexString(eh_frame_header_.version) << "\n";
    ss << "eh_frame_ptr_enc: " << ShowDW_EH_PE(eh_frame_header_.eh_frame_ptr_enc) << "\n";
    ss << "fde_count_enc: " << ShowDW_EH_PE(eh_frame_header_.fde_count_enc) << "\n";
    ss << "table_enc: " << ShowDW_EH_PE(eh_frame_header_.table_enc) << "\n";
    ss << "eh_frame_ptr: " << HexString(eh_frame_header_.eh_frame_ptr) << "\n";
    ss << "fde_count: " << eh_frame_header_.fde_count << "\n";
    for (int i = 0; i < eh_frame_header_.fde_count; i++) {
        ss << "---------- table[" << i << "] ----------\n";
        ss << "initial_loc: " << HexString(eh_frame_header_.table[i].initial_loc) << "\n";
        ss << "fde_ptr: " << HexString(eh_frame_header_.table[i].fde_ptr) << "\n";
        ss << "cie.length: " << HexString(eh_frame_header_.cies[i].length) << "\n";
        ss << "cie.CIE_id: " << HexString(eh_frame_header_.cies[i].CIE_id) << "\n";
        ss << "cie.version: " << HexString(eh_frame_header_.cies[i].version) << "\n";
        ss << "cie.FDE_encoding: " << ShowDW_EH_PE(eh_frame_header_.cies[i].FDE_encoding) << "\n";
        ss << "cie.LSDA_encoding: " << ShowDW_EH_PE(eh_frame_header_.cies[i].LSDA_encoding) << "\n";
        ss << "fde.length: " << HexString(eh_frame_header_.fdes[i].length) << "\n";
        ss << "fde.CIE_delta: " << HexString(eh_frame_header_.fdes[i].CIE_delta) << "\n";
        ss << "fde.initial_loc: " << HexString(eh_frame_header_.fdes[i].initial_loc) << "\n";
    }

    return ss.str();
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
            versym_ = reinterpret_cast<Elf_Versym*>(get_ptr());
        } else if (dyn->d_tag == DT_VERNEED) {
            verneed_ = reinterpret_cast<Elf_Verneed*>(get_ptr());
        } else if (dyn->d_tag == DT_VERNEEDNUM) {
            verneednum_ = dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_VERDEF) {
            verdef_ = reinterpret_cast<Elf_Verdef*>(get_ptr());
        } else if (dyn->d_tag == DT_VERDEFNUM) {
            verdefnum_ = dyn->d_un.d_val;
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
            soname_ = strtab_ + dyn->d_un.d_val;
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

Elf_Addr ELFBinary::OffsetFromAddr(Elf_Addr addr) const {
    for (Elf_Phdr* phdr : loads_) {
        if (phdr->p_vaddr <= addr && addr < phdr->p_vaddr + phdr->p_memsz) {
            return addr - phdr->p_vaddr + phdr->p_offset;
        }
    }
    if (tls() != nullptr && tls()->p_vaddr == addr) {
        return tls()->p_offset;
    }
    LOG(FATAL) << "Address " << HexString(addr, 16) << " cannot be resolved";
}

Elf_Addr ELFBinary::AddrFromOffset(Elf_Addr offset) const {
    for (Elf_Phdr* phdr : loads_) {
        if (phdr->p_offset <= offset && offset < phdr->p_offset + phdr->p_filesz) {
            return offset - phdr->p_offset + phdr->p_vaddr;
        }
    }
    if (tls() != nullptr && tls()->p_offset == offset) {
        return tls()->p_vaddr;
    }
    LOG(FATAL) << "Offset " << HexString(offset, 16) << " cannot be resolved";
}

std::string ELFBinary::ShowDynSymtab() {
    LOG(INFO) << "ShowDynSymtab";
    std::stringstream ss;
    for (auto it : syms_) {
        ss << it.name << ": ";

        if (it.versym == NO_VERSION_INFO) {
            ss << "NO_VERSION_INFO";
        } else if (is_special_ver_ndx(it.versym)) {
            ss << special_ver_ndx_to_str(it.versym);
        } else {
            ss << it.soname << " " << it.version;
        }
        ss << "\n";
    }

    return ss.str();
}

std::string ELFBinary::ShowDtRela() {
    LOG(INFO) << "ShowDtRela";
    CHECK(rel_);
    std::stringstream ss;
    ss << "num_rels_ = " << num_rels_ << std::endl;

    for (int offset = 0; offset < num_rels_; offset++) {
        const Elf_Rel* rp = rel_ + offset;
        const Elf_Sym* sym = &symtab_[ELF_R_SYM(rp->r_info)];
        ss << "r_offset = " << rp->r_offset << ", r_info = " << rp->r_info << ", r_addend = " << rp->r_addend
           << ", symbol name = " << std::string(strtab_ + sym->st_name) << std::endl;
    }

    return ss.str();
}

std::string ELFBinary::ShowTLS() {
    LOG(INFO) << "ShowTLS";
    std::stringstream ss;

    ss << "p_offset = " << tls_->p_offset << std::endl;
    ss << "p_vaddr = " << tls_->p_vaddr << std::endl;
    ss << "p_paddr = " << tls_->p_paddr << std::endl;
    ss << "p_filesz = " << tls_->p_filesz << std::endl;
    ss << "p_memsz = " << tls_->p_memsz << std::endl;
    ss << "p_flags = " << tls_->p_flags << std::endl;
    ss << "p_align = " << tls_->p_align << std::endl;

    return ss.str();
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
        if (p[EI_CLASS] != ELFCLASS64) {
            // TODO(hamaji): Non 64bit ELF isn't supported yet.
            return nullptr;
        }
        return std::make_unique<ELFBinary>(filename.c_str(), fd, p, mapped_size);
    }
    err(1, "unknown file format: %s", filename.c_str());
}
