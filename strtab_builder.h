#pragma once

#include <map>
#include <string>

class StrtabBuilder {
public:
    uintptr_t Add(const std::string& s);

    uintptr_t GetPos(const std::string& s);

    void Freeze() { is_freezed_ = true; }

    size_t size() const { return strtab_.size(); }

    const void* data() const { return strtab_.data(); }

private:
    std::string strtab_;
    std::map<std::string, uintptr_t> cache;
    bool is_freezed_{false};
};
