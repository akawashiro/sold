#! /bin/bash -u

unexpected_failed_tests=
unexpected_succeeded_tests=

# Failed tests
# tls-lib-gcc-aarch64 setjmp-gcc-aarch64 stb_gnu_unique_tls-aarch64 exception-g++-aarch64 tls-multiple-module-g++-aarch64 static-in-class-g++-aarch64 static-in-function-g++-aarch64 tls-dlopen-gcc-aarch64 dynamic_cast-g++-aarch64 typeid-g++-aarch64 inheritance-g++-aarch64 call_once-g++-aarch64 tls-thread-g++-aarch64 tls-multiple-lib-gcc-aarch64 tls-lib-gcc-without-base-aarch64 

for dir in hello-g++ hello-gcc just-return-g++ just-return-gcc simple-lib-g++ simple-lib-gcc version-gcc tls-lib-gcc tls-lib-gcc-without-base tls-multiple-lib-gcc tls-thread-g++ call_once-g++ inheritance-g++ typeid-g++ dynamic_cast-g++ tls-dlopen-gcc static-in-function-g++ static-in-class-g++ tls-multiple-module-g++ exception-g++ stb_gnu_unique_tls setjmp-gcc tls-bss-gcc tls-bss-g++ hello-g++-aarch64 hello-gcc-aarch64 just-return-g++-aarch64 simple-lib-g++-aarch64 simple-lib-gcc-aarch64 version-gcc-aarch64 tls-bss-gcc-aarch64 tls-bss-g++-aarch64 just-return-gcc-aarch64 setjmp-gcc-aarch64 exception-g++-aarch64 typeid-g++-aarch64 inheritance-g++-aarch64 dynamic_cast-g++-aarch64 static-in-class-g++-aarch64 static-in-function-g++-aarch64 
do
    pushd `pwd`
    cd $dir
    echo =========== $dir ===========
    ./test.sh
    if [ "$?" -ne 0 ];then
        if [ -z "${unexpected_failed_tests}" ];then
            unexpected_failed_tests=${dir}
        else
            unexpected_failed_tests=${unexpected_failed_tests},${dir}
        fi
    fi
    popd
done

for dir in tls-lib-gcc-aarch64 stb_gnu_unique_tls-aarch64 tls-multiple-module-g++-aarch64 tls-dlopen-gcc-aarch64 call_once-g++-aarch64 tls-thread-g++-aarch64 tls-multiple-lib-gcc-aarch64 tls-lib-gcc-without-base-aarch64
do
    pushd `pwd`
    cd $dir
    echo =========== $dir ===========
    ./test.sh
    if [ "$?" -eq 0 ];then
        if [ -z "${unexpected_succeeded_tests}" ];then
            unexpected_succeeded_tests=${dir}
        else
            unexpected_succeeded_tests=${unexpected_succeeded_tests},${dir}
        fi
    fi
    popd
done

echo Unexpected failed tests: ${unexpected_failed_tests} 
echo Unexpected succeeded tests: ${unexpected_succeeded_tests}

if [ -z "${unexpected_failed_tests}" ]
then
    exit 0
else
    exit 1
fi
