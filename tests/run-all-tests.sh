#! /bin/bash -eu

for dir in hello-g++ hello-gcc just-return-g++ just-return-gcc simple-lib-g++ simple-lib-gcc version-gcc tls-lib-gcc tls-lib-gcc-without-base tls-multiple-lib-gcc tls-thread-g++ call_once-g++ inheritance-g++ typeid-g++
do
    pushd `pwd`
    cd $dir
    echo =========== $dir ===========
    ./test.sh
    popd
done
