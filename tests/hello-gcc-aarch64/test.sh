#! /bin/bash

aarch64-linux-gnu-gcc hello.c -o hello
../../build/sold hello -o hello.out --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib --custom-library-path /usr/aarch64-linux-gnu/lib64
qemu-aarch64 -L /usr/aarch64-linux-gnu ./hello.out
