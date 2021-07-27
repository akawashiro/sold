#! /bin/bash -eu

gcc -fPIC -c -o libmax.o libmax.c
gcc -Wl,--hash-style=gnu -shared -Wl,-soname,libmax.so -o libmax.so libmax.o
gcc -Wl,--hash-style=gnu -o main.out main.c libmax.so

GLOG_log_dir=. LD_LIBRARY_PATH=. ../../build/sold main.out -o main.soldout --section-headers
LD_LIBRARY_PATH=. ./main.soldout
