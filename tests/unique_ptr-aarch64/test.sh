#! /bin/bash -eux

aarch64-linux-gnu-g++-10 -fPIC -shared -o libhoge.so -Wl,-soname,libhoge.so hoge.cc
aarch64-linux-gnu-g++-10 -o main main.cc libhoge.so

mv libhoge.so libhoge.so.original
GLOG_log_dir=. ../../build/sold -i libhoge.so.original -o libhoge.so.soldout --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib
ln -sf libhoge.so.soldout libhoge.so

LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
