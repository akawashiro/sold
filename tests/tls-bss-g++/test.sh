#! /bin/bash -eu

g++ -Wl,--hash-style=gnu -o main main.cc

../../build/sold main -o main.out --section-headers
../../build/print_dynsymtab main.out
../../build/print_tls main.out
./main.out
