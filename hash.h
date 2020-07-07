#pragma once

#include "utils.h"

struct Elf_GnuHash {
    uint32_t nbuckets{0};
    uint32_t symndx{0};
    uint32_t maskwords{0};
    uint32_t shift2{0};
    uint8_t tail[1];

    Elf_Addr* bloom_filter() { return reinterpret_cast<Elf_Addr*>(tail); }

    uint32_t* buckets() { return reinterpret_cast<uint32_t*>(&bloom_filter()[maskwords]); }

    uint32_t* hashvals() { return reinterpret_cast<uint32_t*>(&buckets()[nbuckets]); }
};

uint32_t CalcGnuHash(const std::string& name);

uint32_t CalcHash(const std::string& name);

struct Elf_Hash {
    uint32_t nbuckets{0};
    uint32_t nchains{0};
    uint8_t tail[0];

    const uint32_t* buckets() const { return reinterpret_cast<const uint32_t*>(tail); }

    const uint32_t* chains() const { return buckets() + nchains; }
};