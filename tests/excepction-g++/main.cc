#include "lib.h"

#include <iostream>

int main() {
    try {
        throw_exeption();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
