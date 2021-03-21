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

## Test with libtorch
```
mkdir -p build
cd build
cmake -DCMAKE_PREFIX_PATH=/absolute/path/to/libtorch/dir  -DSOLD_LIBTORCH_TEST=ON -GNinja ..
ninja
```
