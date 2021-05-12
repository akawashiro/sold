#! /bin/bash

gcc -pie -fPIE hello.c -o hello
../../build/sold hello -o hello.out --section-headers --check-output 
./hello.out
