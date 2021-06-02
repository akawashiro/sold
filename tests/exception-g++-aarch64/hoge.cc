#include "hoge.h"
#include "fuga.h"

void throw_exception_hoge() {
    throw std::runtime_error("hoge");
}

void catch_exception_hoge() {
    try {
        throw_exception_hoge();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        throw_exception_fuga();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
