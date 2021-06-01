#! /bin/bash -eu

aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -o main.out main.c

../../build/sold main.out -o main.soldout --section-headers --custom-library-path /usr/aarch64-linux-gnu/lib
qemu-aarch64 -L /usr/aarch64-linux-gnu ./main.soldout
