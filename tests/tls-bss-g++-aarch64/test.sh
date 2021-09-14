#! /bin/bash -eu

aarch64-linux-gnu-g++ -o main.out main.cc
../../build/sold main.out -o main.soldout --section-headers --custom-library-path /usr/aarch64-linux-gnu/lib --custom-library-path /usr/aarch64-linux-gnu/lib64
qemu-aarch64 -L /usr/aarch64-linux-gnu ./main.soldout
