#! /bin/bash -eu

gcc -fPIC -c -o libmax1.o libmax1.c
gcc -shared -Wl,-soname,libmax.so -o libmax.so.1 libmax1.o
ln -sf libmax.so.1 libmax.so
gcc -pie -fPIE -o vertest1 vertest1.c libmax.so

gcc -fPIC -c -o libmax2.o libmax2.c
gcc -shared -Wl,-soname,libmax.so -Wl,--version-script,libmax2.def -o libmax.so.2 libmax2.o
ln -sf libmax.so.2 libmax.so
gcc -pie -fPIE -o vertest2 vertest2.c libmax.so

LD_LIBRARY_PATH=. ../../build/sold -e libmax.so -o vertest1.out vertest1 --section-headers --check-output 
LD_LIBRARY_PATH=. ./vertest1.out
LD_LIBRARY_PATH=. ../../build/sold -e libmax.so -o vertest2.out vertest2  --section-headers --check-output
LD_LIBRARY_PATH=. ./vertest2.out
