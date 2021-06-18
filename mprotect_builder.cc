#include "mprotect_builder.h"

#include <bitset>
#include <iomanip>
#include <iostream>
#include <vector>

constexpr uint8_t MprotectBuilder::memprotect_body_code_x86_64[];
constexpr uint8_t MprotectBuilder::memprotect_end_code_x86_64[];
constexpr uint8_t MprotectBuilder::memprotect_body_code_aarch64[];
constexpr uint8_t MprotectBuilder::memprotect_end_code_aarch64[];

void MprotectBuilder::EmitX86_64(FILE* fp, uintptr_t mprotect_code_offset) {
    uint8_t* mprotect_code = (uint8_t*)malloc(Size());
    uint8_t* mprotect_code_head = mprotect_code;
    for (int i = 0; i < offsets.size(); i++) {
        LOG(INFO) << "MprotectBuilder::Emit: " << SOLD_LOG_BITS(offsets[i]) << SOLD_LOG_BITS(sizes[i]);

        memcpy(mprotect_code_head, memprotect_body_code_x86_64, sizeof(memprotect_body_code_x86_64));
        int64_t* offset_p = (int64_t*)(mprotect_code_head + memprotect_body_addr_offset_x86_64);
        // 17 is the length of `mov $0xdeadbeefdeadbeef, %rdi; lea (%rip), %rsi'
        int64_t offset_v = (offsets[i] & (~(0x1000 - 1))) -
                           (static_cast<int64_t>(mprotect_code_offset) + static_cast<int64_t>(mprotect_code_head - mprotect_code) + 17);
        LOG(INFO) << "MprotectBuilder::Emit" << SOLD_LOG_BITS(offset_v) << SOLD_LOG_KEY(offset_v);
        *offset_p = offset_v;
        uint32_t* size_p = (uint32_t*)(mprotect_code_head + memprotect_body_size_offset_x86_64);
        *size_p = ((offsets[i] + sizes[i]) & (~(0x1000 - 1))) - (offsets[i] & (~(0x1000 - 1)));
        mprotect_code_head += sizeof(memprotect_body_code_x86_64);
    }
    memcpy(mprotect_code_head, memprotect_end_code_x86_64, sizeof(memprotect_end_code_x86_64));
    mprotect_code_head += sizeof(memprotect_end_code_x86_64);

    CHECK(fwrite(mprotect_code, sizeof(uint8_t), Size(), fp) == Size());
    free(mprotect_code);
}

namespace {
std::vector<uint32_t> set_immediate_to_register(int64_t value, uint8_t reg) {
    std::vector<uint32_t> ret;
    for (int i = 0; i < 4; i++) {
        uint32_t u = 0b11110010'10000000'00000000'00000000;  // movk instruction
        u |= (i << 21);
        u |= ((value >> (i * 16)) & 0xffff) << 5;
        u |= reg;
        ret.emplace_back(u);
    }
    return ret;
}
}  // namespace

void MprotectBuilder::EmitAarch64(FILE* fp, uintptr_t mprotect_code_offset) {
    long int old_pos = ftell(fp);

    LOG(INFO) << "MprotectBuilder::EmitAarch64";
    std::vector<uint32_t> insts;
    for (int i = 0; i < offsets.size(); i++) {
        // 5 is the length of x0 <- offsets[i] using 4 movk instructions; adr x3, 0
        int64_t offset = offsets[i] - (static_cast<int64_t>(mprotect_code_offset) + body_code_length_aarch64 + 5 * 4);
        auto set_x0 = set_immediate_to_register(offset, 0);         // x0 <- offsets[i] using 4 movk instructions
        insts.insert(insts.end(), set_x0.begin(), set_x0.end());    //
        insts.emplace_back(0b00010000'00000000'00000000'00000011);  // adr x3, 0
        insts.emplace_back(0x8B030000);                             // add x0, x0,x3
        auto set_x1 = set_immediate_to_register(sizes[i], 1);       // x1 <- len[i] using 4 movk instructions
        insts.insert(insts.end(), set_x1.begin(), set_x1.end());    //
        insts.emplace_back(0xD2800022);                             // mov x2, 1(PROT_READ)
        insts.emplace_back(0x52800148);                             // mov w8, 10(mprotect)
        insts.emplace_back(0xD4000001);                             // svc 0
    }
    insts.emplace_back(0xD65F03C0);

    for (uint32_t i : insts) {
        CHECK(fwrite(&i, sizeof(uint32_t), 1, fp) == 1);
    }
    CHECK(ftell(fp) - old_pos == Size());
}

