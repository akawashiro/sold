#include <cstdio>

// OK
// No symbol
// int inline_fn() {
//     thread_local int c = 0;
//     return c++;
// }

// OK
// No symbol
// static int inline_fn() {
//     thread_local int c = 0;
//     return c++;
// }

// OK
// OBJECT UNIQUE
// Deleted by sold
// inline int inline_fn() {
//     static int c = 0;
//     return c++;
// }

// NG
// TLS UNIQUE
inline int inline_fn() {
    thread_local int c = 0;
    return c++;
}

// NG
// TLS UNIQUE
// inline thread_local int x = 10;
// int inline_fn() {
//     return x;
// }

// NG
// TLS UNIQUE
// inline int inline_fn() {
//     static thread_local int c = 0;
//     return c++;
// }

int fn() {
    printf("%d\n", inline_fn());
    printf("%d\n", inline_fn());
    return 0;
}
