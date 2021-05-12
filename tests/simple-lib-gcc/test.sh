#! /bin/bash -eu

gcc -fPIC -c -o libmax.o libmax.c
gcc -shared -Wl,-soname,libmax.so -o libmax.so libmax.o
gcc -pie -fPIE -o main.out main.c libmax.so

LD_LIBRARY_PATH=. ../../build/sold main.out -o main.soldout --section-headers --check-output
LD_LIBRARY_PATH=. ./main.soldout
