#! /bin/bash -eux

gcc hoge.c -fPIC -shared -Wl,-soname,libhoge.so -o libhoge.so
gcc main.c libhoge.so
GLOG_log_dir=. LD_LIBRARY_PATH=. ../../build/sold a.out -o a.out.soldout --section-headers
./a.out.soldout
