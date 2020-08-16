#include "shdr_builder.h"

#include "utils.h"

void ShdrBuilder::EmitShstrtab(FILE* fp) {
    std::string str = "\0";
    for (const auto& i : type_to_str) {
        str += i.second;
        str += '\0';
    }

    CHECK(fwrite(str.c_str(), sizeof(str.size()), 1, fp) == 1);
}

uintptr_t ShdrBuilder::SizeOfShstrtab() const {
    std::string str = "\0";
    for (const auto& i : type_to_str) {
        str += i.second;
        str += '\0';
    }
    return str.size();
}
