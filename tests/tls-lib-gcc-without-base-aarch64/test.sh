#! /bin/bash -eu

aarch64-linux-gnu-gcc-10 -fPIC -c -o lib.o lib.c
aarch64-linux-gnu-gcc-10 -fPIC -c -o base.o base.c
aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -shared -Wl,-soname,base.so -o original/base.so base.o
aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -shared -Wl,-soname,lib.so -o original/lib.so lib.o original/base.so
aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -o main.out main.c original/lib.so original/base.so

cp original/base.so sold_out/base.so

LD_LIBRARY_PATH=original ../../build/sold original/lib.so -o sold_out/lib.so --section-headers --exclude-so base.so --check-output --custom-library-path /usr/aarch64-linux-gnu/lib
LD_LIBRARY_PATH=sold_out qemu-aarch64 -L /usr/aarch64-linux-gnu ./main.out
