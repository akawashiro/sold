#! /bin/bash -e

g++ -fPIC -c -o libmax1.o libmax1.cc
g++ -Wl,--hash-style=gnu -shared -Wl,-soname,libmax.so -o libmax.so.1 libmax1.o
ln -sf libmax.so.1 libmax.so
g++ -Wl,--hash-style=gnu -o vertest1 vertest1.cc libmax.so

g++ -fPIC -c -o libmax2.o libmax2.cc
g++ -Wl,--hash-style=gnu -shared -Wl,-soname,libmax.so -Wl,--version-script,libmax2.def -o libmax.so.2 libmax2.o
ln -sf libmax.so.2 libmax.so
g++ -Wl,--hash-style=gnu -o vertest2 vertest2.cc libmax.so

LD_LIBRARY_PATH=. ../../build/sold vertest1 vertest1.out
LD_LIBRARY_PATH=. ./vertest1.out
LD_LIBRARY_PATH=. ../../build/sold vertest2 vertest2.out
LD_LIBRARY_PATH=. ./vertest2.out
