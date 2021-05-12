#! /bin/bash -eu

g++ -pie -fPIE -o main.out main.cc
../../build/sold main.out -o main.soldout --section-headers
./main.soldout
