#! /bin/bash -eux

g++ -std=c++17 -fPIC -shared unique.cc -o libunique.so
g++ -std=c++17 -o main main.cc libunique.so

mv libunique.so libunique.so.original
../../build/sold -i libunique.so.original -o libunique.so.soldout --section-headers --check-output
ln -sf libunique.so.soldout libunique.so

./main
