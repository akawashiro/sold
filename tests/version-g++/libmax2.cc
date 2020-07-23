#include <iostream>

int max__1(int a, int b){
    std::cout << "max__1 @ libmax2" << std::endl;
    return (a > b ? a : b);
}

int max__2(int a, int b, int c){
    std::cout << "max__2 @ libmax2" << std::endl;
    return (a < b ? a : b);
}

__asm__(".symver max__1,max@LIBMAX_1.0");
__asm__(".symver max__2,max@@LIBMAX_2.0");
