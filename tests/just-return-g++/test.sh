#! /bin/bash

g++ -pie -fPIE return.cc -o return.out
../../build/sold return.out -o return.soldout --section-headers --check-output
./return.soldout
