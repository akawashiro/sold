#include "utils.h"

bool QUIET_LOG{false};

namespace {

bool ShouldUseColor() {
    auto should_use_color_fn = []() {
        const char* env_ptr = getenv("SOLD_COLOR");
        if (!env_ptr) return true;
        const std::string env(env_ptr);
        // MN_CHECK_ONE_OF(env, "always", "auto", "never", "");
        if (env == "auto") {
            return isatty(STDIN_FILENO) == 1;
        } else if (env == "never") {
            return false;
        } else if (env == "always") {
            return true;
        }
    };
    static bool should_use_color = should_use_color_fn();
    return should_use_color;
}

}  // namespace

const std::string& LogGreen() {
    static const std::string color = "\033[92m";
    static const std::string empty = "";
    return ShouldUseColor() ? color : empty;
}

const std::string& LogBlue() {
    static const std::string color = "\033[94m";
    static const std::string empty = "";
    return ShouldUseColor() ? color : empty;
}

const std::string& LogYellow() {
    static const std::string color = "\033[93m";
    static const std::string empty = "";
    return ShouldUseColor() ? color : empty;
}

const std::string& LogRed() {
    static const std::string color = "\033[91m";
    static const std::string empty = "";
    return ShouldUseColor() ? color : empty;
}

const std::string& LogReset() {
    static const std::string color = "\033[0m";
    static const std::string empty = "";
    return ShouldUseColor() ? color : empty;
}

std::string LogGreen(const std::string& msg) {
    return StrCat(LogGreen(), msg, LogReset());
}

std::string LogBlue(const std::string& msg) {
    return StrCat(LogBlue(), msg, LogReset());
}

std::string LogYellow(const std::string& msg) {
    return StrCat(LogYellow(), msg, LogReset());
}

std::string LogRed(const std::string& msg) {
    return StrCat(LogRed(), msg, LogReset());
}

std::vector<std::string> SplitString(const std::string& str, const std::string& sep) {
    std::vector<std::string> ret;
    if (str.empty()) return ret;
    size_t index = 0;
    while (true) {
        size_t next = str.find(sep, index);
        ret.push_back(str.substr(index, next - index));
        if (next == std::string::npos) break;
        index = next + 1;
    }
    return ret;
}

bool HasPrefix(const std::string& str, const std::string& prefix) {
    ssize_t size_diff = str.size() - prefix.size();
    return size_diff >= 0 && str.substr(0, prefix.size()) == prefix;
}

uintptr_t AlignNext(uintptr_t a, uintptr_t mask) {
    return (a + mask) & ~mask;
}

bool IsTLS(const Elf_Sym& sym) {
    return ELF_ST_TYPE(sym.st_info) == STT_TLS;
}

bool IsDefined(const Elf_Sym& sym) {
    return (sym.st_value || IsTLS(sym)) && sym.st_shndx != SHN_UNDEF;
}

bool is_special_ver_ndx(Elf64_Versym versym) {
    return (versym == VER_NDX_LOCAL || versym == VER_NDX_GLOBAL);
}

std::string special_ver_ndx_to_str(Elf64_Versym versym) {
    if (versym == VER_NDX_LOCAL) {
        return std::string("VER_NDX_LOCAL");
    } else if (versym == VER_NDX_GLOBAL) {
        return std::string("VER_NDX_GLOBAL");
    } else {
        LOGF("This versym (= %d) is not special.\n", versym);
        exit(1);
    }
}
