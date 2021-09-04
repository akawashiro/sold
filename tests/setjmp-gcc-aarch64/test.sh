#! /bin/bash -eux

aarch64-linux-gnu-gcc -shared -fPIC -Wl,-soname,libhoge.so -o libhoge.so hoge.c
aarch64-linux-gnu-gcc -o main main.c libhoge.so

mv libhoge.so libhoge.so.original
../../build/sold -i libhoge.so.original -o libhoge.so.soldout --section-headers  --custom-library-path /usr/aarch64-linux-gnu/lib --custom-library-path /usr/aarch64-linux-gnu/lib64

# Use sold
ln -sf libhoge.so.soldout libhoge.so

# Use original
# ln -sf lib.so.original lib.so

LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
