#! /bin/bash -eux

aarch64-linux-gnu-gcc -fPIC -shared -o libhoge.so -Wl,-soname,libhoge.so hoge.c
aarch64-linux-gnu-gcc -o main main.c libhoge.so

mv libhoge.so libhoge.so.original
../../build/sold -i libhoge.so.original -o libhoge.so.soldout --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib --custom-library-path /usr/aarch64-linux-gnu/lib64
ln -sf libhoge.so.soldout libhoge.so

LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
