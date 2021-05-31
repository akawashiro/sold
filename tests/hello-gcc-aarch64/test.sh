#! /bin/bash

aarch64-linux-gnu-gcc hello.c -o hello
aarch64-linux-gnu-gcc hello.c -S -o hello.S
qemu-aarch64 -L /usr/aarch64-linux-gnu ./hello
# ../../build/sold hello -o hello.out --section-headers --check-output 
# ./hello.out
