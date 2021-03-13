#! /bin/bash -eu

gcc -shared -Wl,-soname,base.so -o original/base.so base.c
gcc -shared -Wl,-soname,lib.so -o original/lib.so lib.c original/base.so
gcc -shared -Wl,-soname,lib.so -o answer/lib.so lib.c base.c

# Without sold
echo ----- without sold -----
LD_LIBRARY_PATH=original gcc -o main-answer main.c answer/lib.so
LD_LIBRARY_PATH=original ./main-answer

# With sold
echo ----- with sold -----
GLOG_log_dir=. LD_LIBRARY_PATH=original ../../build/sold original/lib.so -o sold_out/lib.so --section-headers --check-output
LD_LIBRARY_PATH=sold_out gcc -o main-soldout main.c sold_out/lib.so
LD_LIBRARY_PATH=sold_out ./main-soldout
