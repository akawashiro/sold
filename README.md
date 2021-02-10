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

## Compile
```
mkdir -p build
cd build
cmake .. -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPYBIND11_PYTHON_VERSION=$(python3 -c "import platform; print(platform.python_version())")
ninja
```

## Test
```
python3 pybind_test_sold.py
```
