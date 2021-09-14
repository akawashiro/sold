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

#include "utils.h"
#include <iomanip>

std::vector<std::string> SplitString(const std::string& str, const std::string& sep) {
    std::vector<std::string> ret;
    if (str.empty()) return ret;
    size_t index = 0;
    while (true) {
        size_t next = str.find(sep, index);
        ret.push_back(str.substr(index, next - index));
        if (next == std::string::npos) break;
        index = next + 1;
    }
    return ret;
}

bool HasPrefix(const std::string& str, const std::string& prefix) {
    ssize_t size_diff = str.size() - prefix.size();
    return size_diff >= 0 && str.substr(0, prefix.size()) == prefix;
}

uintptr_t AlignNext(uintptr_t a, uintptr_t mask) {
    return (a + mask) & ~mask;
}

void WriteBuf(FILE* fp, const void* buf, size_t size) {
    CHECK(fwrite(buf, 1, size, fp) == size);
}

void EmitZeros(FILE* fp, uintptr_t cnt) {
    std::string zero(cnt, '\0');
    WriteBuf(fp, zero.data(), zero.size());
}

void EmitPad(FILE* fp, uintptr_t to) {
    uint pos = ftell(fp);
    CHECK_GE(pos, 0);
    CHECK_LE(pos, to);
    EmitZeros(fp, to - pos);
}

void EmitAlign(FILE* fp) {
    long pos = ftell(fp);
    CHECK(pos >= 0);
    EmitZeros(fp, AlignNext(pos) - pos);
}

bool IsTLS(const Elf_Sym& sym) {
    return ELF_ST_TYPE(sym.st_info) == STT_TLS;
}

bool IsDefined(const Elf_Sym& sym) {
    return (sym.st_value || IsTLS(sym)) && sym.st_shndx != SHN_UNDEF;
}

std::ostream& operator<<(std::ostream& os, const Syminfo& s) {
    auto f = os.flags();
    os << "Syminfo{name=" << s.name << ", soname=" << s.soname << ", version=" << s.version << ", versym=" << s.versym << ", sym=0x"
       << std::hex << std::setfill('0') << std::setw(16) << s.sym << "}";
    os.flags(f);
    return os;
}

std::string ShowDW_EH_PE(uint8_t type) {
    if (type == DW_EH_PE_omit) {
        return "DW_EH_PE_omit";
    }

    std::string ret;

    switch (type & 0xf) {
        case DW_EH_PE_absptr:
            ret = "DW_EH_PE_absptr";
            break;
        case DW_EH_PE_uleb128:
            ret = "DW_EH_PE_uleb128";
            break;
        case DW_EH_PE_udata2:
            ret = "DW_EH_PE_udata2";
            break;
        case DW_EH_PE_udata4:
            ret = "DW_EH_PE_udata4";
            break;
        case DW_EH_PE_udata8:
            ret = "DW_EH_PE_udata8";
            break;
        case DW_EH_PE_sleb128:
            ret = "DW_EH_PE_sleb128";
            break;
        case DW_EH_PE_sdata2:
            ret = "DW_EH_PE_sdata2";
            break;
        case DW_EH_PE_sdata4:
            ret = "DW_EH_PE_sdata4";
            break;
        case DW_EH_PE_sdata8:
            ret = "DW_EH_PE_sdata8";
            break;
    }

    switch (type & 0xf0) {
        case DW_EH_PE_pcrel:
            ret += " + DW_EH_PE_pcrel";
            break;
        case DW_EH_PE_textrel:
            ret += " + DW_EH_PE_textrel";
            break;
        case DW_EH_PE_datarel:
            ret += " + DW_EH_PE_datarel";
            break;
        case DW_EH_PE_funcrel:
            ret += " + DW_EH_PE_funcrel";
            break;
        case DW_EH_PE_aligned:
            ret += " + DW_EH_PE_aligned";
            break;
    }

    if (type == DW_EH_PE_SOLD_DUMMY) {
        ret = "DW_EH_PE_SOLD_DUMMY(0xEE)";
    } else if (ret == "") {
        ret = HexString(type, 2);
    }

    return ret;
}

std::string ShowRelocationType(int type) {
    switch (type) {
        case R_X86_64_NONE:
            return "R_X86_64_NONE";
        case R_X86_64_64:
            return "R_X86_64_64";
        case R_X86_64_PC32:
            return "R_X86_64_PC32";
        case R_X86_64_GOT32:
            return "R_X86_64_GOT32";
        case R_X86_64_PLT32:
            return "R_X86_64_PLT32";
        case R_X86_64_COPY:
            return "R_X86_64_COPY";
        case R_X86_64_GLOB_DAT:
            return "R_X86_64_GLOB_DAT";
        case R_X86_64_JUMP_SLOT:
            return "R_X86_64_JUMP_SLOT";
        case R_X86_64_RELATIVE:
            return "R_X86_64_RELATIVE";
        case R_X86_64_GOTPCREL:
            return "R_X86_64_GOTPCREL";
        case R_X86_64_32:
            return "R_X86_64_32";
        case R_X86_64_32S:
            return "R_X86_64_32S";
        case R_X86_64_16:
            return "R_X86_64_16";
        case R_X86_64_PC16:
            return "R_X86_64_PC16";
        case R_X86_64_8:
            return "R_X86_64_8";
        case R_X86_64_PC8:
            return "R_X86_64_PC8";
        case R_X86_64_DTPMOD64:
            return "R_X86_64_DTPMOD64";
        case R_X86_64_DTPOFF64:
            return "R_X86_64_DTPOFF64";
        case R_X86_64_TPOFF64:
            return "R_X86_64_TPOFF64";
        case R_X86_64_TLSGD:
            return "R_X86_64_TLSGD";
        case R_X86_64_TLSLD:
            return "R_X86_64_TLSLD";
        case R_X86_64_DTPOFF32:
            return "R_X86_64_DTPOFF32";
        case R_X86_64_GOTTPOFF:
            return "R_X86_64_GOTTPOFF";
        case R_X86_64_TPOFF32:
            return "R_X86_64_TPOFF32";
        case R_X86_64_PC64:
            return "R_X86_64_PC64";
        case R_X86_64_GOTOFF64:
            return "R_X86_64_GOTOFF64";
        case R_X86_64_GOTPC32:
            return "R_X86_64_GOTPC32";
        case R_X86_64_GOT64:
            return "R_X86_64_GOT64";
        case R_X86_64_GOTPCREL64:
            return "R_X86_64_GOTPCREL64";
        case R_X86_64_GOTPC64:
            return "R_X86_64_GOTPC64";
        case R_X86_64_GOTPLT64:
            return "R_X86_64_GOTPLT64";
        case R_X86_64_PLTOFF64:
            return "R_X86_64_PLTOFF64";
        case R_X86_64_SIZE32:
            return "R_X86_64_SIZE32";
        case R_X86_64_SIZE64:
            return "R_X86_64_SIZE64";
        case R_X86_64_GOTPC32_TLSDESC:
            return "R_X86_64_GOTPC32_TLSDESC";
        case R_X86_64_TLSDESC:
            return "R_X86_64_TLSDESC";
        case R_X86_64_IRELATIVE:
            return "R_X86_64_IRELATIVE";
        case R_X86_64_RELATIVE64:
            return "R_X86_64_RELATIVE64";
        case R_X86_64_GOTPCRELX:
            return "R_X86_64_GOTPCRELX";
        case R_X86_64_REX_GOTPCRELX:
            return "R_X86_64_REX_GOTPCRELX";
        case R_X86_64_NUM:
            return "R_X86_64_NUM";
        default: {
            return HexString(type, 4);
        }
    }
}

std::ostream& operator<<(std::ostream& os, const Elf_Rel& r) {
    os << "Elf_Rela{r_offset=" << SOLD_LOG_32BITS(r.r_offset) << ", r_info=" << SOLD_LOG_32BITS(r.r_info)
       << ", ELF_R_SYM(r.r_info)=" << SOLD_LOG_16BITS(ELF_R_SYM(r.r_info))
       << ", ELF_R_TYPE(r.r_info)=" << ShowRelocationType(ELF_R_TYPE(r.r_info)) << ", r_addend=" << SOLD_LOG_32BITS(r.r_addend) << "}";
    return os;
}

bool is_special_ver_ndx(Elf64_Versym versym) {
    return (versym == VER_NDX_LOCAL || versym == VER_NDX_GLOBAL);
}

std::string special_ver_ndx_to_str(Elf64_Versym versym) {
    if (versym == VER_NDX_LOCAL) {
        return std::string("VER_NDX_LOCAL");
    } else if (versym == VER_NDX_GLOBAL) {
        return std::string("VER_NDX_GLOBAL");
    } else if (versym == NO_VERSION_INFO) {
        return std::string("NO_VERSION_INFO");
    } else {
        LOG(FATAL) << "This versym (= " << versym << ") is not special.";
        exit(1);
    }
}

// Copy from sysdeps/generic/unwind-pe.h in glibc
const char* read_uleb128(const char* p, uint32_t* val) {
    unsigned int shift = 0;
    unsigned char byte;
    uint32_t result;

    result = 0;
    do {
        byte = *p++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    *val = result;
    return p;
}

// Copy from sysdeps/generic/unwind-pe.h in glibc
const char* read_sleb128(const char* p, int32_t* val) {
    unsigned int shift = 0;
    unsigned char byte;
    int32_t result;

    result = 0;
    do {
        byte = *p++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    /* Sign-extend a negative value.  */
    if (shift < 8 * sizeof(result) && (byte & 0x40) != 0) result |= -(1L << shift);

    *val = (int32_t)result;
    return p;
}

// Copy from sysdeps/generic/unwind-pe.h in glibc
typedef unsigned sold_Unwind_Internal_Ptr __attribute__((__mode__(__pointer__)));
#define DW_EH_PE_indirect 0x80

const char* read_encoded_value_with_base(unsigned char encoding, sold_Unwind_Ptr base, const char* p, uint32_t* val) {
    union unaligned {
        void* ptr;
        unsigned u2 __attribute__((mode(HI)));
        unsigned u4 __attribute__((mode(SI)));
        unsigned u8 __attribute__((mode(DI)));
        signed s2 __attribute__((mode(HI)));
        signed s4 __attribute__((mode(SI)));
        signed s8 __attribute__((mode(DI)));
    } __attribute__((__packed__));

    union unaligned* u = (union unaligned*)p;
    sold_Unwind_Internal_Ptr result;

    if (encoding == DW_EH_PE_aligned) {
        sold_Unwind_Internal_Ptr a = (sold_Unwind_Internal_Ptr)p;
        a = (a + sizeof(void*) - 1) & -sizeof(void*);
        result = *(sold_Unwind_Internal_Ptr*)a;
        p = (char*)(a + sizeof(void*));
    } else {
        switch (encoding & 0x0f) {
            case DW_EH_PE_absptr:
                result = (sold_Unwind_Internal_Ptr)u->ptr;
                p += sizeof(void*);
                break;

            case DW_EH_PE_uleb128: {
                uint32_t tmp;
                p = read_uleb128(p, &tmp);
                result = (sold_Unwind_Internal_Ptr)tmp;
            } break;

            case DW_EH_PE_sleb128: {
                int32_t tmp;
                p = read_sleb128(p, &tmp);
                result = (sold_Unwind_Internal_Ptr)tmp;
            } break;

            case DW_EH_PE_udata2:
                result = u->u2;
                p += 2;
                break;
            case DW_EH_PE_udata4:
                result = u->u4;
                p += 4;
                break;
            case DW_EH_PE_udata8:
                result = u->u8;
                p += 8;
                break;

            case DW_EH_PE_sdata2:
                result = u->s2;
                p += 2;
                break;
            case DW_EH_PE_sdata4:
                result = u->s4;
                p += 4;
                break;
            case DW_EH_PE_sdata8:
                result = u->s8;
                p += 8;
                break;
            default:
                LOG(FATAL) << SOLD_LOG_8BITS(encoding & 0x0f);
        }

        if (result != 0) {
            result += ((encoding & 0x70) == DW_EH_PE_pcrel ? (sold_Unwind_Internal_Ptr)u : base);
            if (encoding & DW_EH_PE_indirect) result = *(sold_Unwind_Internal_Ptr*)result;
        }
    }

    *val = result;
    return p;
}
