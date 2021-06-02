#! /bin/bash -eux

aarch64-linux-gnu-g++-10 -std=c++17 -fPIC -shared unique.cc -o libunique.so
aarch64-linux-gnu-g++-10 -std=c++17 -o main main.cc libunique.so

mv libunique.so libunique.so.original
../../build/sold -i libunique.so.original -o libunique.so.soldout --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib
ln -sf libunique.so.soldout libunique.so

LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
