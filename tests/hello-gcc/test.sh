#! /bin/bash

gcc hello.c -o hello
../../build/sold hello hello.out
./hello.out
