#! /bin/bash

aarch64-linux-gnu-g++-10 return.cc -o return.out
../../build/sold return.out -o return.soldout --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib
qemu-aarch64 -L /usr/aarch64-linux-gnu ./return.soldout
