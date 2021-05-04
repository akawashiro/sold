cmake_minimum_required(VERSION 3.4)

pybind11_add_module(myadd add.cc)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/soldout")
add_custom_target(myadd_soldout ALL
    COMMAND "${PROJECT_BINARY_DIR}/sold" -i "${CMAKE_CURRENT_BINARY_DIR}/myadd.cpython-39-x86_64-linux-gnu.so" -o "${CMAKE_CURRENT_BINARY_DIR}/soldout/myadd.cpython-39-x86_64-linux-gnu.so" --section-headers --check-output
    DEPENDS sold)
configure_file(test_soldout_modules.py ${CMAKE_CURRENT_BINARY_DIR}/soldout/test_soldout_modules.py @ONLY)
