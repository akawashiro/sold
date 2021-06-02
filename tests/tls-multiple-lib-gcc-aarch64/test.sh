#! /bin/bash -eu

aarch64-linux-gnu-gcc-10 -fPIC -c -o base.o base.c
aarch64-linux-gnu-gcc-10 -fPIC -c -o base2.o base2.c
aarch64-linux-gnu-gcc-10 -fPIC -c -o lib.o lib.c
aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -shared -Wl,-soname,base.so -o original/base.so base.o
aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -shared -Wl,-soname,base2.so -o original/base2.so base2.o
aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -shared -Wl,-soname,lib.so -o original/lib.so lib.o original/base2.so original/base.so
aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -shared -Wl,-soname,lib.so -o answer/lib.so lib.o base2.o base.o

# Without sold
# LD_LIBRARY_PATH=original gcc -Wl,--hash-style=gnu -o main.out main.c original/lib.so original/base2.so original/base.so
# LD_LIBRARY_PATH=original ./main.out

LD_LIBRARY_PATH=original ../../build/sold original/lib.so -o sold_out/lib.so --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib
 
LD_LIBRARY_PATH=sold_out aarch64-linux-gnu-gcc-10 -Wl,--hash-style=gnu -o main.out main.c sold_out/lib.so
LD_LIBRARY_PATH=sold_out qemu-aarch64 -L /usr/aarch64-linux-gnu ./main.out
