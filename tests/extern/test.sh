#! /bin/bash -eux

pushd ~/sold/build
ninja
popd
gcc hoge.c -fPIC -shared -Wl,-soname,libhoge.so -o libhoge.so
gcc main.c libhoge.so -rdynamic -export-dynamic
# GLOG_logtostderr=1 ../../build/print_dynsymtab libhoge.so

GLOG_log_dir=. LD_LIBRARY_PATH=. ../../build/sold a.out -o a.out.soldout --section-headers
GLOG_logtostderr=1 LD_LIBRARY_PATH=. ../../build/sold a.out -o a.out.soldout --section-headers
./a.out.soldout
