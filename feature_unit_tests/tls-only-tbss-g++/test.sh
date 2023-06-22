#! /bin/bash -eu

g++ -shared -fPIC -o libfuga.so -Wl,-soname,libfuga.so fuga.cc
g++ -shared -fPIC -o libhoge.so -Wl,-soname,libhoge.so hoge.cc
g++ -shared -fPIC -o libfugahoge.so.original fugahoge.cc libfuga.so libhoge.so
g++ -o main main.cc -ldl 

LD_LIBRARY_PATH=. $(git rev-parse --show-toplevel)/build/sold -i libfugahoge.so.original -o libfugahoge.so.soldout --section-headers --check-output

LD_LIBRARY_PATH=. ./main
