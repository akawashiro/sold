#! /bin/bash -eu

g++ -fPIC -shared -o libfuga.so -Wl,-soname,libfuga.so fuga.cc 
g++ -fPIC -shared -o libhoge.so.original -Wl,-soname,libhoge.so hoge.cc libfuga.so
g++ -pie -fPIE main.cc -o main.out libhoge.so.original libfuga.so
LD_LIBRARY_PATH=. ../../build/sold -i libhoge.so.original -o libhoge.so.soldout --section-headers --check-output

# Use sold
ln -sf libhoge.so.soldout libhoge.so
# Use original
# ln -sf libhoge.so.original libhoge.so

LD_LIBRARY_PATH=. ./main.out
