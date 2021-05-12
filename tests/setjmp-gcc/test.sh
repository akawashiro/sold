#! /bin/bash -eux

gcc -shared -fPIC -Wl,-soname,libhoge.so -o libhoge.so hoge.c
gcc -pie -fPIE -o main main.c libhoge.so

mv libhoge.so libhoge.so.original
../../build/sold -i libhoge.so.original -o libhoge.so.soldout --section-headers

# Use sold
ln -sf libhoge.so.soldout libhoge.so

# Use original
# ln -sf lib.so.original lib.so

LD_LIBRARY_PATH=. ./main
