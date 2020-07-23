#! /bin/bash -eu

g++ -fPIC -c -o libmax.o libmax.cc
g++ -Wl,--hash-style=gnu -shared -Wl,-soname,libmax.so -o libmax.so libmax.o
g++ -Wl,--hash-style=gnu -o main main.cc libmax.so

LD_LIBRARY_PATH=. ../../build/sold main main.out
LD_LIBRARY_PATH=. ./main.out
