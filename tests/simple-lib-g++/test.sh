#! /bin/bash -eu

g++ -fPIC -c -o libmax.o libmax.cc
g++ -shared -Wl,-soname,libmax.so -o libmax.so libmax.o
g++ -pie -fPIE -o main.out main.cc libmax.so

LD_LIBRARY_PATH=. ../../build/sold main.out -o main.soldout --section-headers --check-output
LD_LIBRARY_PATH=. ./main.soldout
