#! /bin/bash -eu

gcc -fPIC -c -o lib.o lib.c
gcc -Wl,--hash-style=gnu -shared -Wl,-soname,lib.so -o lib.so lib.o
gcc -Wl,--hash-style=gnu -o main main.c lib.so

mv lib.so lib.so.original
../../build/sold -i lib.so.original -o lib.so.soldout --section-headers

# Use sold
ln -sf lib.so.soldout lib.so

# Use original
# ln -sf lib.so.original lib.so

./main
