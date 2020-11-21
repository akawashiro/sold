#include "lib.h"

int main() {
    std::cout << "catch_exception" << std::endl;
    catch_exception();

    std::cout << "throw_exception" << std::endl;
    try {
        throw_exception();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
