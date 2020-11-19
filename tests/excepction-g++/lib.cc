#include "lib.h"

void throw_exception() {
    throw std::runtime_error("Hello");
}

void catch_exception() {
    try {
        throw_exception();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
