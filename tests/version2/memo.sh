#! /bin/bash

gcc -c -Wall -Werror -fpic libcmp.c
ld -shared libcmp.o -o libcmp.so

ld --version-script version.map -shared libcmp.o -o libcmp.ver.so
