#include "hoge.h"

int main() {
    std::cout << "catch_exception" << std::endl;
    catch_exception_hoge();

    std::cout << "throw_exception" << std::endl;
    try {
        throw_exception_hoge();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
