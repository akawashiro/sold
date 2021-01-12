#! /bin/bash -eu

gcc -fPIC -shared -Wl,-soname,libhoge.so -o libhoge.so hoge.c
gcc -o main main.c libhoge.so

mv libhoge.so libhoge.so.original
# ../../build/sold -i libhoge.so.original -o libhoge.so.soldout --section-headers
../../build/sold -i libhoge.so.original -o libhoge.so.soldout

# Use sold
ln -sf libhoge.so.soldout libhoge.so

# Use original
# ln -sf libhoge.so.original libhoge.so

./main
