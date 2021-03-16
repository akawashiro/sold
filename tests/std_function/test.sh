#! /bin/bash -eux

g++ -fPIC -shared -o libhoge.so -Wl,-soname,libhoge.so hoge.cc
g++ -fPIC -shared -o libfuga.so -Wl,-soname,libfuga.so fuga.cc libhoge.so
g++ -o main main.cc libfuga.so

mv libfuga.so libfuga.so.original
LD_LIBRARY_PATH=. ../../build/sold -i libfuga.so.original -o libfuga.so.soldout --section-headers --check-output
ln -sf libfuga.so.soldout libfuga.so

LD_LIBRARY_PATH=. ./main
