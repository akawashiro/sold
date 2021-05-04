cmake_minimum_required(VERSION 3.4)

pybind11_add_module(myadd add.cc)
pybind11_add_module(myobject object.cc)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/soldout")
add_custom_target(myadd_soldout ALL
    COMMAND "${PROJECT_BINARY_DIR}/sold" -i "${CMAKE_CURRENT_BINARY_DIR}/myadd.cpython-39-x86_64-linux-gnu.so" -o "${CMAKE_CURRENT_BINARY_DIR}/soldout/myadd.cpython-39-x86_64-linux-gnu.so" --section-headers --check-output
    DEPENDS sold myadd)
add_custom_target(myobject_soldout ALL
    COMMAND "${PROJECT_BINARY_DIR}/sold" -i "${CMAKE_CURRENT_BINARY_DIR}/myobject.cpython-39-x86_64-linux-gnu.so" -o "${CMAKE_CURRENT_BINARY_DIR}/soldout/myobject.cpython-39-x86_64-linux-gnu.so" --section-headers --check-output
    DEPENDS sold myobject)

configure_file(test_soldout_modules.py ${CMAKE_CURRENT_BINARY_DIR}/test_soldout_modules_original.py @ONLY)
configure_file(test_soldout_modules.py ${CMAKE_CURRENT_BINARY_DIR}/soldout/test_soldout_modules.py @ONLY)

add_test(NAME pybind_test WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/soldout" COMMAND pytest -v)
