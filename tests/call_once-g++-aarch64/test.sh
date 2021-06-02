#! /bin/bash -eux

arch64-linux-gnu-g++-10 -fPIC -shared -Wl,-soname,libhoge.so -o libhoge.so hoge.cc
arch64-linux-gnu-g++-10 -fPIC -shared -Wl,-soname,libfuga.so -o libfuga.so fuga.cc libhoge.so
LD_LIBRARY_PATH=. aarch64-linux-gnu-g++-10 -o main main.cc libfuga.so -pthread

mv libfuga.so libfuga.so.original
GLOG_log_dir=. LD_LIBRARY_PATH=. ../../build/sold -i libfuga.so.original -o libfuga.so.soldout --section-headers --custom-library-path /usr/aarch64-linux-gnu/lib
ln -sf libfuga.so.soldout libfuga.so
LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
