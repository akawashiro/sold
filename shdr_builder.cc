#include "shdr_builder.h"

#include <elf.h>
#include "utils.h"

void ShdrBuilder::EmitShstrtab(FILE* fp) {
    std::string str = "";
    str += '\0';
    for (const auto& i : type_to_str) {
        str += i.second;
        str += '\0';
    }

    CHECK(fwrite(str.c_str(), str.size(), 1, fp) == 1);
}

uintptr_t ShdrBuilder::ShstrtabSize() const {
    std::string str = "";
    str += '\0';
    for (const auto& i : type_to_str) {
        str += i.second;
        str += '\0';
    }
    return str.size();
}

uint32_t ShdrBuilder::GetShName(ShdrType type) const {
    // The head of shstrtab is '\0' so we must skip it.
    uint32_t r = 1;
    for (const auto& i : type_to_str) {
        if (i.first == type) {
            break;
        } else {
            r += i.second.size() + 1;
        }
    }
    return r;
}

void ShdrBuilder::EmitShdrs(FILE* fp) {
    LOGF("EmitShdrs\n");

    Elf_Shdr shstrtab;
    bool found_shstrtab = false;
    int num_not_shstrtab = 0;

    // Emit other than shstrtab
    for (const auto& s : shdrs) {
        if (s.sh_name == GetShName(Shstrtab)) {
            shstrtab = s;
            found_shstrtab = true;
        } else {
            num_not_shstrtab++;
            CHECK(fwrite(&s, sizeof(s), 1, fp) == 1);
        }
    }

    // num_not_shstrtab must not be 0 because ehdr_.e_shstrndx is ignored when it is 0
    CHECK(found_shstrtab);
    CHECK(num_not_shstrtab != 0);

    CHECK(fwrite(&shstrtab, sizeof(shstrtab), 1, fp) == 1);
}

void ShdrBuilder::RegisterShdr(Elf_Off offset, uint64_t size, ShdrType type) {
    Elf_Shdr shdr = {0};
    shdr.sh_name = GetShName(type);
    shdr.sh_offset = offset;
    shdr.sh_size = size;
    switch (type) {
        case Shstrtab: {
            shdr.sh_type = SHT_STRTAB;
            shdr.sh_flags = 0;
            shdr.sh_addr = 0;
            break;
        }
        case Dynamic: {
            shdr.sh_type = SHT_DYNAMIC;
            shdr.sh_flags = 0;
            shdr.sh_addr = 0;
            break;
        }
        case Dynstr: {
            shdr.sh_type = SHT_STRTAB;
            shdr.sh_flags = 0;
            shdr.sh_addr = 0;
            break;
        }
        default:
            LOGF("Not implemented\n");
            exit(1);
    }
    shdrs.push_back(shdr);
}
