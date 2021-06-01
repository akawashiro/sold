#include "lib.h"

int X::i_static = 42;
double X::d_static = 42.00;

double func() {
    X x;
    return x.f();
}
