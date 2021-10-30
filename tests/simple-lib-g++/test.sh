#! /bin/bash -eu

g++ -fPIC -c -o libmax.o libmax.cc
g++ -Wl,--hash-style=gnu -shared -Wl,-soname,libmax.so -o libmax.so libmax.o
g++ -Wl,--hash-style=gnu -o main.out main.cc libmax.so

GLOG_logtostderr=1 LD_LIBRARY_PATH=. ../../build/sold main.out -o main.soldout --section-headers --check-output
LD_LIBRARY_PATH=. ./main.soldout
