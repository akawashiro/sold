#include "fuga.h"

void throw_exception_fuga() {
    throw std::runtime_error("fuga");
}

void catch_exception_fuga() {
    try {
        throw_exception_fuga();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
