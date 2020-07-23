#! /bin/bash

g++ hello.cc -o hello
../../build/sold hello hello.out
./hello.out
