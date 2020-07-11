#include "strtab_builder.h"

#include "utils.h"

uintptr_t StrtabBuilder::Add(const std::string& s) {
    CHECK(!is_freezed_);
    if (cache.find(s) != cache.end()) {
        return cache[s];
    }
    uintptr_t pos = static_cast<uintptr_t>(strtab_.size());
    strtab_ += s;
    strtab_ += '\0';
    cache[s] = pos;
    return pos;
}
