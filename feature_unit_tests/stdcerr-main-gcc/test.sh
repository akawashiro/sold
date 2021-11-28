#! /bin/bash -eux

gcc -o libhoge.so -shared -fpic -fPIC -Wl,-soname,libhoge.so hoge.c
gcc -o main main.c libhoge.so
GLOG_log_dir=. LD_LIBRARY_PATH=. $(git rev-parse --show-toplevel)/build/sold -i main -o main.soldout --section-headers

LD_LIBRARY_PATH=. ./main.soldout
