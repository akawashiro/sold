#! /bin/bash -eu

gcc -fPIC -c -o libmax.o libmax.c
gcc -Wl,--hash-style=gnu -shared -Wl,-soname,libmax.so -o libmax.so libmax.o
gcc -Wl,--hash-style=gnu -o main main.c libmax.so

LD_LIBRARY_PATH=. ../../build/sold main main.out
LD_LIBRARY_PATH=. ./main.out
