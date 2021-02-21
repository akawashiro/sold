#! /bin/bash -u

ret_code=0
failed_tests=

for dir in hello-g++ hello-gcc just-return-g++ just-return-gcc simple-lib-g++ simple-lib-gcc version-gcc tls-lib-gcc tls-lib-gcc-without-base tls-multiple-lib-gcc tls-thread-g++ call_once-g++ inheritance-g++ typeid-g++ dynamic_cast-g++ tls-dlopen-gcc static-in-function-g++ static-in-class-g++ tls-multiple-module-g++ exception-g++
do
    pushd `pwd`
    cd $dir
    echo =========== $dir ===========
    ./test.sh
    if [ "$?" -ne 0 ];then
        ret_code=1
        if [ -z "${failed_tests}" ];then
            failed_tests=${dir}
        else
            failed_tests=${dir},${failed_tests}
        fi
    fi
    popd
done

echo Failed tests: ${failed_tests} 
exit ${ret_code}
