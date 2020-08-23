#! /bin/bash -eu

gcc -fPIC -c -o lib.o lib.c
gcc -Wl,--hash-style=gnu -shared -Wl,-soname,lib.so -o lib.so lib.o
gcc -Wl,--hash-style=gnu -o main main.c lib.so

LD_LIBRARY_PATH=. ../../build/sold main -o main.out --section-headers
../../build/print_dynsymtab main.out
../../build/print_tls main.out
./main.out
