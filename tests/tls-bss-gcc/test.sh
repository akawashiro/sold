#! /bin/bash -eu

gcc -pie -fPIE -o main.out main.c

../../build/sold main.out -o main.soldout --section-headers
./main.soldout
