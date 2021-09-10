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

#include <map>
#include <string>
#include <vector>

class StrtabBuilder {
public:
    StrtabBuilder() {}
    StrtabBuilder(std::map<std::string, std::string> rename_mapping) : rename_mapping_(rename_mapping) {}

    uintptr_t Add(const std::string& s);

    uintptr_t GetPos(const std::string& s);

    void Freeze() { is_freezed_ = true; }

    size_t size() const { return strtab_.size(); }

    const void* data() const { return strtab_.data(); }
    const std::vector<std::string> strs() const { return strs_; }

private:
    std::string strtab_;
    std::vector<std::string> strs_;
    std::map<std::string, uintptr_t> cache;
    bool is_freezed_{false};
    std::map<std::string, std::string> rename_mapping_;
};
