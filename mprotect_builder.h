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

#pragma once

#include <stdint.h>
#include <vector>

#include "utils.h"

class MprotectBuilder {
public:
    void SetMachineType(const Elf64_Half machine_type) {
        CHECK(machine_type == EM_X86_64 || machine_type == EM_AARCH64);
        machine_type_ = machine_type;
    }
    void Add(uintptr_t offset, uintptr_t size) {
        offsets.emplace_back(offset);
        sizes.emplace_back(size);
    }
    uintptr_t Size() const {
        CHECK(offsets.size() == sizes.size());
        if (machine_type_ == EM_X86_64) {
            return sizeof(memprotect_body_code_x86_64) * offsets.size() + sizeof(memprotect_end_code_x86_64);
        } else if (machine_type_ == EM_AARCH64) {
            // return sizeof(memprotect_body_code_aarch64) * offsets.size() + sizeof(memprotect_end_code_aarch64);
            return body_code_length_aarch64 * offsets.size() + ret_code_length_aarch64;
        } else {
            CHECK(false) << SOLD_LOG_KEY(machine_type_) << " is not supported.";
        }
    }
    void Emit(FILE* fp, uintptr_t mprotect_code_offset) {
        CHECK(machine_type_ == EM_X86_64 || machine_type_ == EM_AARCH64) << "SetMachineType before Emit";
        LOG(INFO) << SOLD_LOG_BITS(fp) << SOLD_LOG_BITS(mprotect_code_offset);
        if (machine_type_ == EM_X86_64) {
            EmitX86_64(fp, mprotect_code_offset);
        } else if (machine_type_ == EM_AARCH64) {
            // Dummy code
            // uint8_t* mprotect_code = (uint8_t*)malloc(Size());
            // uint8_t* mprotect_code_head = mprotect_code;
            // for (int i = 0; i < offsets.size(); i++) {
            //     memcpy(mprotect_code_head, memprotect_body_code_aarch64, sizeof(memprotect_body_code_aarch64));
            //     mprotect_code_head += sizeof(memprotect_body_code_aarch64);
            // }
            // memcpy(mprotect_code_head, memprotect_end_code_aarch64, sizeof(memprotect_end_code_aarch64));
            // mprotect_code_head += sizeof(memprotect_end_code_aarch64);
            // CHECK(fwrite(mprotect_code, sizeof(uint8_t), Size(), fp) == Size());
            // free(mprotect_code);
            FILE* oldfp = fp;
            EmitAarch64(fp, mprotect_code_offset);
            CHECK_EQ(fp - oldfp, Size());
        } else {
            CHECK(false) << SOLD_LOG_KEY(machine_type_) << " is not supported.";
        }
    }

    // call SYS_mprotect syscall
    //
    // mov $0xdeadbeefdeadbeef, %rdi
    // lea (%rip), %rsi
    // add %rsi, %rdi
    // mov $0xaabbcc, %rsi (size)
    // mov $0x1, %rdx (0x1 = PROT_READ)
    // mov $10, %eax (10 = SYS_mprotect)
    // syscall
    // test %eax, %eax
    // jz ok
    // ud2
    // ok:
    static constexpr uint8_t memprotect_body_code_x86_64[] = {0x48, 0xbf, 0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde, 0x48, 0x8d,
                                                              0x35, 0x00, 0x00, 0x00, 0x00, 0x48, 0x01, 0xf7, 0x48, 0xc7, 0xc6, 0xcc,
                                                              0xbb, 0xaa, 0x00, 0x48, 0xc7, 0xc2, 0x01, 0x00, 0x00, 0x00, 0xb8, 0x0a,
                                                              0x00, 0x00, 0x00, 0x0f, 0x05, 0x85, 0xc0, 0x74, 0x02, 0x0f, 0x0b};
    // offset to 0xabbccdd
    static constexpr int memprotect_body_addr_offset_x86_64 = 2;
    // offset to 0xaabbcc
    static constexpr int memprotect_body_size_offset_x86_64 = 23;

    // ret
    static constexpr uint8_t memprotect_end_code_x86_64[] = {0xc3};

    // just nop
    static constexpr uint8_t memprotect_body_code_aarch64[] = {0x1f, 0x20, 0x03, 0xd5};

    // ret
    static constexpr uint8_t memprotect_end_code_aarch64[] = {0xc0, 0x03, 0x5f, 0xd6};

    static constexpr int body_code_length_aarch64 = 13 * 4;
    static constexpr int ret_code_length_aarch64 = 4;

private:
    void EmitX86_64(FILE* fp, uintptr_t mprotect_code_offset);
    void EmitAarch64(FILE* fp, uintptr_t mprotect_code_offset);
    Elf64_Half machine_type_;
    std::vector<int64_t> offsets;
    std::vector<uint32_t> sizes;
};
