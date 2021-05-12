#! /bin/bash -eu

gcc -fPIC -c -o lib.o lib.c
gcc -fPIC -c -o base.o base.c
gcc -shared -Wl,-soname,base.so -o original/base.so base.o
gcc -shared -Wl,-soname,lib.so -o original/lib.so lib.o original/base.so
gcc -shared -Wl,-soname,lib.so -o answer/lib.so lib.o base.o

# Without sold
# LD_LIBRARY_PATH=original gcc -pie -fPIE -o main.out main.c original/lib.so original/base.so
# LD_LIBRARY_PATH=original ./main.out

LD_LIBRARY_PATH=original ../../build/sold original/lib.so -o sold_out/lib.so --section-headers --check-output
 
LD_LIBRARY_PATH=sold_out gcc -pie -fPIE -o main.out main.c sold_out/lib.so
LD_LIBRARY_PATH=sold_out ./main.out

