#! /bin/bash -eux

aarch64-linux-gnu-g++-10 -fPIC -shared -o libfuga.so -Wl,-soname,libfuga.so fuga.cc 
aarch64-linux-gnu-g++-10 -fPIC -shared -o libhoge.so -Wl,-soname,libhoge.so hoge.cc libfuga.so
aarch64-linux-gnu-g++-10 -o main main.cc libhoge.so

LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
