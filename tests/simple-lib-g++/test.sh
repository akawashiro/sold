#! /bin/bash -eu

g++ -fstack-protector-all -fPIC -shared max.cc -o libmax.so -Wl,-soname,libmax.so
g++ -fstack-protector-all -o main main.cc libmax.so

LD_LIBRARY_PATH=. ../../build/sold main -o main.out --section-headers
LD_LIBRARY_PATH=. ./main.out
