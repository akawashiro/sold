#include "fuga.h"

void throw_exception_fuga() {
    std::cout << "QWERHOGETYUIOPASDFGHJKLZXCVBNM" << std::endl;
    throw std::runtime_error("fuga");
}

void catch_exception_fuga() {
    try {
        std::cout << "QWERTYUIOPASDFGHJKLZXCVBNM" << std::endl;
        throw_exception_fuga();
    } catch (std::exception& e) {
        std::cout << "QWERHOGETYUIOPASDFGHJKLZXCVBNM" << std::endl;
        std::cout << e.what() << std::endl;
    }
}
