#! /bin/bash -eu

aarch64-linux-gnu-g++ -fPIC -c -o lib.o lib.cc
aarch64-linux-gnu-g++ -Wl,--hash-style=gnu -shared -Wl,-soname,lib.so -o lib.so lib.o
aarch64-linux-gnu-g++ -Wl,--hash-style=gnu -o main main.cc lib.so

mv lib.so lib.so.original
../../build/sold -i lib.so.original -o lib.so.soldout --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib

# Use sold
ln -sf lib.so.soldout lib.so
echo ----------- Use sold -----------
LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main

# Use original
ln -sf lib.so.original lib.so
echo ----------- Use original -----------
LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
