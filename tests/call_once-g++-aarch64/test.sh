#! /bin/bash -eux

aarch64-linux-gnu-g++ -fPIC -shared -Wl,-soname,libhoge.so -o libhoge.so hoge.cc
aarch64-linux-gnu-g++ -fPIC -shared -Wl,-soname,libfuga.so -o libfuga.so fuga.cc libhoge.so
LD_LIBRARY_PATH=. aarch64-linux-gnu-g++ -o main main.cc libfuga.so libhoge.so -pthread

mv libfuga.so libfuga.so.original
GLOG_log_dir=. LD_LIBRARY_PATH=. ../../build/sold -i libfuga.so.original -o libfuga.so.soldout --section-headers --custom-library-path /usr/aarch64-linux-gnu/lib --custom-library-path /usr/aarch64-linux-gnu/lib64
ln -sf libfuga.so.soldout libfuga.so
LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
