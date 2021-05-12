#! /bin/bash -eu

gcc -pie -fPIE -o main main.c

../../build/sold main -o main.out --section-headers
./main.out
