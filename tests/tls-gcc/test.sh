#! /bin/bash -eu

gcc -Wl,--hash-style=gnu -o main main.c

../../build/sold main -o main.out --section-headers
../../build/print_dynsymtab main.out
../../build/print_tls main.out
./main.out
