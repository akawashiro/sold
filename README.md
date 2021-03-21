# sold
`sold` is a linker software which links a shared objects and its depending
shared objects. For example,
```bash
% ldd libhoge.so
    linux-vdso.so.1 (0x00007ffee03bb000)
    libfuga.so (0x00007fb6550b2000)
    libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007fb654eb5000)
    libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fb654cc3000)
    libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007fb654b74000)
    /lib64/ld-linux-x86-64.so.2 (0x00007fb6550be000)
    libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007fb654b59000)
% sold -i libhoge.so -o libhoge2.so
% ldd libhoge2.so
    /lib64/ld-linux-x86-64.so.2 (0x00007f78a666b000)
    linux-vdso.so.1 (0x00007fffd89d1000)
    libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f789644f000)
    libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f7896434000)
    libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f78962e5000)
    libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007f7896104000)
```

## Requirements
`sold` works only on Linux on x86-64 architecture.

## How to build
```bash
git clone https://github.com/shinh/sold.git
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
- `--section-headers`: Emit section headers. Output shared objects work without section headers but they are useful for debugging.
- `--check-output`: Check integrity of the output by parsing it again.
- `--exclude-so`: Specify a shared object not to combine.

# For developers
## TODO
- TLS in executables
- AArch64
- x86-32

## Failed tests
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
