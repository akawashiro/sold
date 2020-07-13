#! /bin/bash -e

gcc -fPIC -c -Wl,--hash-style=sysv -o libcmp2.o libcmp2.c
gcc -shared -Wl,-soname,libcmp.so -Wl,--version-script,libcmp2.def -Wl,--hash-style=sysv -o libcmp.so.1.0 libcmp2.o
ln -s libcmp.so.1.0 libcmp.so
gcc -L. -lcmp -Wl,--hash-style=sysv -o vertest2 vertest2.c
