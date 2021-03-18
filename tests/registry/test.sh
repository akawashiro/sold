#! /bin/bash

# g++ -fPIC -shared -o libimpl.so impl.cc
g++ -fPIC -shared -o libhoge.so hoge.cc impl.cc
g++ -o main main.cc libhoge.so
