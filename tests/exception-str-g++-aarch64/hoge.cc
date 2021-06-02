#include "hoge.h"
#include "fuga.h"

void throw_exception_hoge() {
    throw std::runtime_error("hoge");
}

void catch_exception_hoge() {
    try {
        std::cout << "QWERTYUIOPASDFGHJKLZXCVBNM" << std::endl;
        throw_exception_hoge();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        std::cout << "QWERTYUIOPASDFGHJKLZXCVBNM" << std::endl;
        throw_exception_fuga();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
