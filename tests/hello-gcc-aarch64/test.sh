#! /bin/bash

aarch64-linux-gnu-gcc-10 hello.c -o hello
../../build/sold hello -o hello.out --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib
qemu-aarch64 -L /usr/aarch64-linux-gnu ./hello.out
