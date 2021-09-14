// read_uleb128, read_sleb128, sold_Unwind_Ptr, read_encoded_value_with_base
// were copied from sysdeps/generic/unwind-pe.h in glibc. Free Software
// Foundation, Inc. holds the copyright for them.  The sold authors hold the
// copyright for the other part of this source code.
//
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
//
// Copyright (C) 2012-2021 Free Software Foundation, Inc.
// Written by Ian Lance Taylor, Google.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     (1) Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//
//     (2) Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in
//     the documentation and/or other materials provided with the
//     distribution.
//
//     (3) The name of the author may not be used to
//     endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <elf.h>
#include <stddef.h>

#include <cassert>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <glog/logging.h>

#define SOLD_LOG_KEY_VALUE(key, value) " " << key << "=" << value
#define SOLD_LOG_KEY(key) SOLD_LOG_KEY_VALUE(#key, key)
#define SOLD_LOG_64BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 16))
#define SOLD_LOG_32BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 8))
#define SOLD_LOG_16BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 4))
#define SOLD_LOG_8BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 2))
#define SOLD_LOG_BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key))
#define SOLD_LOG_DWEHPE(type) SOLD_LOG_KEY_VALUE(#type, ShowDW_EH_PE(type))
#define SOLD_CHECK_EQ(a, b) CHECK(a == b) << SOLD_LOG_BITS(a) << SOLD_LOG_BITS(b)

#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Dyn Elf64_Dyn
#define Elf_Addr Elf64_Addr
#define Elf_Sym Elf64_Sym
#define Elf_Shdr Elf64_Shdr
#define Elf_Off Elf64_Off
#define Elf_Half Elf64_Half
#define Elf_Versym Elf64_Versym
#define Elf_Xword Elf64_Xword
#define Elf_Word Elf64_Word
#define Elf_Vernaux Elf64_Vernaux
#define Elf_Verdaux Elf64_Verdaux
#define Elf_Verneed Elf64_Verneed
#define Elf_Verdef Elf64_Verdef
#define ELF_ST_BIND(val) ELF64_ST_BIND(val)
#define ELF_ST_TYPE(val) ELF64_ST_TYPE(val)
#define ELF_ST_INFO(bind, type) ELF64_ST_INFO(bind, type)
#define Elf_Rel Elf64_Rela
#define ELF_R_SYM(val) ELF64_R_SYM(val)
#define ELF_R_TYPE(val) ELF64_R_TYPE(val)
#define ELF_R_INFO(sym, type) ELF64_R_INFO(sym, type)

#define DW_EH_PE_absptr 0x00
#define DW_EH_PE_omit 0xff

#define DW_EH_PE_uleb128 0x01
#define DW_EH_PE_udata2 0x02
#define DW_EH_PE_udata4 0x03
#define DW_EH_PE_udata8 0x04
#define DW_EH_PE_sleb128 0x09
#define DW_EH_PE_sdata2 0x0A
#define DW_EH_PE_sdata4 0x0B
#define DW_EH_PE_sdata8 0x0C
#define DW_EH_PE_signed 0x08

#define DW_EH_PE_pcrel 0x10
#define DW_EH_PE_textrel 0x20
#define DW_EH_PE_datarel 0x30
#define DW_EH_PE_funcrel 0x40
#define DW_EH_PE_aligned 0x50

#define DW_EH_PE_indirect 0x80

// 0xee is never used as a valid DWARF Exception Header value
#define DW_EH_PE_SOLD_DUMMY 0xee

// Although there is no description of VERSYM_HIDDEN in glibc, you can find it
// in binutils source code.
// https://github.com/gittup/binutils/blob/8db2e9c8d085222ac7b57272ee263733ae193565/include/elf/common.h#L816
#define VERSYM_HIDDEN 0x8000
#define VERSYM_VERSION 0x7fff

static constexpr uintptr_t LINUX_PAGE_SIZE = 0x1000;
static constexpr Elf_Versym NO_VERSION_INFO = 0xffff;

std::vector<std::string> SplitString(const std::string& str, const std::string& sep);

bool HasPrefix(const std::string& str, const std::string& prefix);

template <class T>
void Write(FILE* fp, const T& v) {
    CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);
}
uintptr_t AlignNext(uintptr_t a, uintptr_t mask = LINUX_PAGE_SIZE - 1);
void WriteBuf(FILE* fp, const void* buf, size_t size);
void EmitZeros(FILE* fp, uintptr_t cnt);
void EmitPad(FILE* fp, uintptr_t to);
void EmitAlign(FILE* fp);

struct Range {
    uintptr_t start;
    uintptr_t end;
    ptrdiff_t size() const { return end - start; }
    Range operator+(uintptr_t offset) const { return Range{start + offset, end + offset}; }
    Range operator-(uintptr_t offset) const { return Range{start - offset, end - offset}; }
};

bool IsTLS(const Elf_Sym& sym);

bool IsDefined(const Elf_Sym& sym);

class ELFBinary;

struct Syminfo {
    std::string name;
    std::string soname;
    std::string version;
    Elf_Versym versym;
    Elf_Sym* sym;
};

std::string ShowDynamicEntryType(int type);
std::string ShowRelocationType(int type);
std::string ShowDW_EH_PE(uint8_t type);
std::ostream& operator<<(std::ostream& os, const Syminfo& s);
std::ostream& operator<<(std::ostream& os, const Elf_Rel& s);

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
    Elf_Xword align{0};
};

struct EHFrameHeader {
    struct FDETableEntry {
        int32_t initial_loc;
        int32_t fde_ptr;
    };

    struct CIE {
        uint32_t length;
        int32_t CIE_id;
        uint8_t version;
        const char* aug_str;
        uint8_t FDE_encoding;
        uint8_t LSDA_encoding;
    };

    struct FDE {
        uint32_t length;
        uint64_t extended_length;
        int32_t CIE_delta;
        int32_t initial_loc;
    };

    uint8_t version;
    uint8_t eh_frame_ptr_enc;
    uint8_t fde_count_enc;
    uint8_t table_enc;

    int32_t eh_frame_ptr;
    uint32_t fde_count;

    std::vector<FDETableEntry> table;
    std::vector<FDE> fdes;
    std::vector<CIE> cies;
};

bool is_special_ver_ndx(Elf64_Versym v);

std::string special_ver_ndx_to_str(Elf_Versym v);

template <class T>
inline std::string HexString(T num, int length = -1) {
    if (length == -1) {
        length = sizeof(T) * 2;
    }
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::setfill('0') << std::setw(length) << std::hex << +num;
    return ss.str();
}

const char* read_uleb128(const char* p, uint32_t* val);
const char* read_sleb128(const char* p, int32_t* val);

typedef unsigned sold_Unwind_Ptr __attribute__((__mode__(__word__)));
const char* read_encoded_value_with_base(unsigned char encoding, sold_Unwind_Ptr base, const char* p, uint32_t* val);
