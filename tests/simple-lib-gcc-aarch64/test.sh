#! /bin/bash -eu

aarch64-linux-gnu-gcc -fPIC -c -o libmax.o libmax.c
aarch64-linux-gnu-gcc -Wl,--hash-style=gnu -shared -Wl,-soname,libmax.so -o libmax.so libmax.o
aarch64-linux-gnu-gcc -Wl,--hash-style=gnu -o main.out main.c libmax.so

LD_LIBRARY_PATH=. ../../build/sold main.out -o main.soldout --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib --custom-library-path /usr/aarch64-linux-gnu/lib64
LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main.soldout
