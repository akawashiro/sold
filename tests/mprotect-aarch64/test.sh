#! /bin/bash

aarch64-linux-gnu-gcc-10 -shared -fPIC base.c -o base.so -Wl,-soname,base.so
aarch64-linux-gnu-gcc-10 -shared -fPIC mprotect_check.c base.so -o mprotect_check.so
aarch64-linux-gnu-gcc-10 main.c mprotect_check.so -o main

LD_LIBRARY_PATH=. ../../build/sold -i mprotect_check.so -o mprotect_check.so.soldout --custom-library-path /usr/aarch64-linux-gnu/lib
mv mprotect_check.so mprotect_check.so.original
ln -sf mprotect_check.so.soldout mprotect_check.so
# ln -sf mprotect_check.so.original mprotect_check.so

LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
