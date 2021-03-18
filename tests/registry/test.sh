#! /bin/bash -eux

g++ -fPIC -shared -Wl,-soname,libinterface.so -o libinterface.so interface.cc
g++ -fPIC -shared -Wl,-soname,libimpl.so -o libimpl.so impl.cc libinterface.so
g++ -fPIC -shared -Wl,-soname,libhoge.so -o libhoge.so hoge.cc libimpl.so libinterface.so
g++ -o main main.cc libhoge.so

# mv libhoge.so libhoge.so.original
# LD_LIBRARY_PATH=. ../../build/sold -i libhoge.so.original -o libhoge.so.soldout --section-headers --check-output
# ln -sf libhoge.so.soldout libhoge.so

./main
