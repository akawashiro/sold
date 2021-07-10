#! /bin/bash -eux

g++ -fPIC -shared -Wl,-soname,libhoge.so -o libhoge.so hoge.cc
g++ -fPIC -shared -Wl,-soname,libfuga.so -o libfuga.so fuga.cc libhoge.so
LD_LIBRARY_PATH=. g++ -o main main.cc libfuga.so libhoge.so -pthread

mv libfuga.so libfuga.so.original
GLOG_log_dir=. LD_LIBRARY_PATH=. ../../build/sold -i libfuga.so.original -o libfuga.so.soldout --section-headers
ln -sf libfuga.so.soldout libfuga.so
LD_LIBRARY_PATH=. ./main
