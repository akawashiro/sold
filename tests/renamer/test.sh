#! /bin/bash -eux

gcc -fPIC -fpic -shared -o libhoge_original.so hoge.c -Wl,-soname,libhoge.so
gcc -fPIC -fpic -shared -o libfugafuga.so fugafuga.c -Wl,-soname,libhoge.so
ln -sf libfugafuga.so libhoge.so
gcc -o use_fugafuga use_fugafuga.c libhoge.so
GLOG_logtostderr=1 ../../build/renamer libhoge_original.so --output libhoge_renamed.so --rename-mapping-file mapping
ln -sf libhoge_renamed.so libhoge.so
./use_fugafuga
