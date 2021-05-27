#! /bin/bash

aarch64-linux-gnu-gcc-9 hello.c -o hello
aarch64-linux-gnu-gcc-9 hello.c -S -o hello.S
qemu-system-aarch64 -L /usr/aarch64-linux-gnu ./hello
# ../../build/sold hello -o hello.out --section-headers --check-output 
# ./hello.out
