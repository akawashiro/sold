#! /bin/bash -eu

gcc -fPIC -c -o lib.o lib.c
gcc -fPIC -c -o base.o base.c
gcc -Wl,--hash-style=gnu -shared -Wl,-soname,base.so -o original/base.so base.o
gcc -Wl,--hash-style=gnu -shared -Wl,-soname,lib.so -o original/lib.so lib.o original/base.so
gcc -Wl,--hash-style=gnu -shared -Wl,-soname,lib.so -o answer/lib.so lib.o base.o

# Without sold
# LD_LIBRARY_PATH=original gcc -Wl,--hash-style=gnu -o main main.c original/lib.so original/base.so
# LD_LIBRARY_PATH=original ./main

LD_LIBRARY_PATH=original ../../build/sold original/lib.so -o sold_out/lib.so --section-headers
 
LD_LIBRARY_PATH=sold_out gcc -Wl,--hash-style=gnu -o main main.c sold_out/lib.so
LD_LIBRARY_PATH=sold_out ./main

# ../../build/print_dynsymtab lib.so.sold
# ../../build/print_tls lib.so.sold
