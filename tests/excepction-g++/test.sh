#! /bin/bash -eu

g++ -fPIC -shared hoge.cc -o libhoge.so
g++ main.cc -o main libhoge.so

# Use sold
# ln -sf lib.so.soldout lib.so

# Use original
# ln -sf lib.so.original lib.so

# ./main
