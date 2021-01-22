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
    void Emit(FILE* fp) {
        CHECK(fwrite(memprotect_start_code, sizeof(uint8_t), sizeof(memprotect_start_code), fp) == sizeof(memprotect_start_code));
        for (int i = 0; i < offsets.size(); i++) {
            CHECK(fwrite(memprotect_body_code, sizeof(uint8_t), sizeof(memprotect_body_code), fp) == sizeof(memprotect_body_code));
        }
        CHECK(fwrite(memprotect_end_code, sizeof(uint8_t), sizeof(memprotect_end_code), fp) == sizeof(memprotect_end_code));
        LOG(INFO) << SOLD_LOG_BITS(sizeof(memprotect_start_code)) << SOLD_LOG_BITS(sizeof(memprotect_body_code))
                  << SOLD_LOG_BITS(sizeof(memprotect_end_code)) << SOLD_LOG_KEY(offsets.size());
    }

    static constexpr uint8_t memprotect_start_code[] = {0xcc, 0xcc, 0xcc, 0xcc};
    static constexpr uint8_t memprotect_body_code[] = {0x48, 0xbf, 0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde, 0x48, 0x03, 0x3d,
                                                       0x00, 0x00, 0x00, 0x00, 0x48, 0xc7, 0xc6, 0xcc, 0xbb, 0xaa, 0x00, 0x48, 0xc7,
                                                       0xc2, 0x01, 0x00, 0x00, 0x00, 0xb8, 0x0a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc3};
    static constexpr uint8_t memprotect_end_code[] = {0x66, 0x66};

private:
    std::vector<uintptr_t> offsets;
    std::vector<uintptr_t> sizes;
};
