#! /bin/bash -eux

cd build
ninja
cd ..

gcc -fPIC -fpic -shared -o libhoge_for_rename_original.so tests/hoge_for_rename.c -Wl,-soname,libhoge_for_rename.so
gcc -fPIC -fpic -shared -o libfugafuga.so tests/fugafuga.c -Wl,-soname,libhoge_for_rename.so
ln -sf libfugafuga.so libhoge_for_rename.so
# ln -sf libhoge_for_rename_original.so libhoge_for_rename.so
gcc -o use_hoge_for_rename tests/use_hoge_for_rename.c libhoge_for_rename.so
GLOG_logtostderr=1 ./build/renamer libhoge_for_rename_original.so --output libhoge_for_rename_renamed.so --rename-mapping-file mapping
ln -sf libhoge_for_rename_renamed.so libhoge_for_rename.so
./use_hoge_for_rename
