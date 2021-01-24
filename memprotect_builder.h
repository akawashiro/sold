#pragma once

#include <stdint.h>
#include <vector>

#include "utils.h"

class MemprotectBuilder {
public:
    void Add(uintptr_t offset, uintptr_t size) {
        offsets.emplace_back(offset);
        sizes.emplace_back(size);
    }
    uintptr_t Size() const {
        return sizeof(memprotect_start_code) + sizeof(memprotect_body_code) * offsets.size() + sizeof(memprotect_end_code);
    }
    void Emit(FILE* fp, uintptr_t mprotect_code_offset) {
        uint8_t* mprotect_code = (uint8_t*)malloc(Size());
        uint8_t* mprotect_code_head = mprotect_code;
        memcpy(mprotect_code_head, memprotect_start_code, sizeof(memprotect_start_code));
        mprotect_code_head += sizeof(memprotect_start_code);
        for (int i = 0; i < offsets.size(); i++) {
            LOG(INFO) << "MemprotectBuilder::Emit: " << SOLD_LOG_BITS(offsets[i]) << SOLD_LOG_BITS(sizes[i]);

            memcpy(mprotect_code_head, memprotect_body_code, sizeof(memprotect_body_code));
            int64_t* offset_p = (int64_t*)(mprotect_code_head + memprotect_body_addr_offset);
            // 17 is the length of `mov $0xdeadbeefdeadbeef, %rdi; lea (%rip), %rsi'
            int64_t offset_v = (offsets[i] & (~(0x1000 - 1))) -
                               (static_cast<int64_t>(mprotect_code_offset) + static_cast<int64_t>(mprotect_code_head - mprotect_code) + 17);
            LOG(INFO) << "MemprotectBuilder::Emit" << SOLD_LOG_BITS(offset_v) << SOLD_LOG_KEY(offset_v);
            *offset_p = offset_v;
            uint32_t* size_p = (uint32_t*)(mprotect_code_head + memprotect_body_size_offset);
            *size_p = ((offsets[i] + sizes[i]) & (~(0x1000 - 1))) - (offsets[i] & (~(0x1000 - 1)));
            mprotect_code_head += sizeof(memprotect_body_code);
        }
        memcpy(mprotect_code_head, memprotect_end_code, sizeof(memprotect_end_code));
        mprotect_code_head += sizeof(memprotect_end_code);

        CHECK(fwrite(mprotect_code, sizeof(uint8_t), Size(), fp) == Size());
        free(mprotect_code);
    }

    // nop
    static constexpr uint8_t memprotect_start_code[] = {0x66};

    // call SYS_mprotect syscall
    //
    // mov $0xdeadbeefdeadbeef, %rdi
    // lea (%rip), %rsi
    // add %rsi, %rdi
    // mov $0xaabbcc, %rsi (size)
    // mov $0x1, %rdx (0x1 = PROT_READ)
    // mov $10, %eax (10 = SYS_mprotect)
    // syscall
    static constexpr uint8_t memprotect_body_code[] = {0x48, 0xbf, 0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde, 0x48, 0x8d, 0x35, 0x00,
                                                       0x00, 0x00, 0x00, 0x48, 0x01, 0xf7, 0x48, 0xc7, 0xc6, 0xcc, 0xbb, 0xaa, 0x00, 0x48,
                                                       0xc7, 0xc2, 0x01, 0x00, 0x00, 0x00, 0xb8, 0x0a, 0x00, 0x00, 0x00, 0x0f, 0x05};
    // offset to 0xabbccdd
    static constexpr int memprotect_body_addr_offset = 2;
    // offset to 0xaabbcc
    static constexpr int memprotect_body_size_offset = 23;

    // ret
    static constexpr uint8_t memprotect_end_code[] = {0xc3};

private:
    std::vector<int64_t> offsets;
    std::vector<uint32_t> sizes;
};
