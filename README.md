# sold
`sold` is a linker software which links a shared objects and its depending
shared objects. For example,
```bash
% ldd libhoge.so
% sold -i libhoge.so -o libhoge2.so
% ldd
```

## Requirements
`sold` is works only on Linux on x86-64 architecture.

## How to build
You need `libdwarf-dev` package in Ubuntu.
```bash
sudo apt install libdwarf-dev
git clone 
cd sold
mkdir -p build
cd build
cmake ..
make
```
Now you can see the `sold` in `build` directory.

## How to use
```bash
sold -i [INPUT] -o [OUTPUT]
```
Options
- `--section-headers`: Emit section headers. This is not needed to work as a shared object but useful for debug.

## TODO
- TLS in executables
- AArch64
- x86-32

## Failed tests
- setjmp-gcc
- tls-bss-g++
- tls-bss-gcc

## Test with Docker
```
sudo docker build -f ubuntu18.04.Dockerfile .
sudo docker build -f ubuntu20.04.Dockerfile .
```
