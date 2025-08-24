#! /bin/bash

set -ux -o pipefail

gcc -o libfail.so -shared -fPIC fail.c
gcc -o main main.c
./main
