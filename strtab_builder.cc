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

#include "strtab_builder.h"

#include "utils.h"

uintptr_t StrtabBuilder::Add(const std::string& s) {
    std::string t = s;
    if (rename_mapping_.find(s) != rename_mapping_.end()) t = rename_mapping_[s];

    CHECK(!is_freezed_);
    if (cache.find(t) != cache.end()) {
        return cache[t];
    }
    uintptr_t pos = static_cast<uintptr_t>(strtab_.size());
    strtab_ += t;
    strtab_ += '\0';
    cache[t] = pos;
    return pos;
}

uintptr_t StrtabBuilder::GetPos(const std::string& s) {
    std::string t = s;
    if (rename_mapping_.find(s) != rename_mapping_.end()) t = rename_mapping_[s];

    if (cache.find(t) != cache.end()) {
        return cache[t];
    } else {
        LOG(FATAL) << t << " is not in StrtabBuilder.";
        exit(1);
    }
}
