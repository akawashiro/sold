#! /bin/bash -eu

aarch64-linux-gnu-g++-10 -o main.out main.cc
../../build/sold main.out -o main.soldout --section-headers --custom-library-path /usr/aarch64-linux-gnu/lib
qemu-aarch64 -L /usr/aarch64-linux-gnu ./main.soldout
