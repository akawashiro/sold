cmake_minimum_required(VERSION 3.4)

# I set PREFIX and SUFFIX here because I want to pass generated files to sold.
# The default SUFFIX depends on the system configuration and is difficult to
# infer.
pybind11_add_module(myadd add.cc)
set_target_properties(myadd PROPERTIES PREFIX "" SUFFIX ".so")
pybind11_add_module(myobject object.cc)
set_target_properties(myobject PROPERTIES PREFIX "" SUFFIX ".so")
pybind11_add_module(mynumpy numpy.cc)
set_target_properties(mynumpy PROPERTIES PREFIX "" SUFFIX ".so")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/soldout")
add_custom_target(
  myadd_soldout ALL
  COMMAND
    "${PROJECT_BINARY_DIR}/sold" -i "${CMAKE_CURRENT_BINARY_DIR}/myadd.so" -o
    "${CMAKE_CURRENT_BINARY_DIR}/soldout/myadd.so" --section-headers
    --check-output
  DEPENDS sold myadd)
add_custom_target(
  myobject_soldout ALL
  COMMAND
    "${PROJECT_BINARY_DIR}/sold" -i "${CMAKE_CURRENT_BINARY_DIR}/myobject.so" -o
    "${CMAKE_CURRENT_BINARY_DIR}/soldout/myobject.so" --section-headers
    --check-output
  DEPENDS sold myobject)
add_custom_target(
  mynumpy_soldout ALL
  COMMAND
    "${PROJECT_BINARY_DIR}/sold" -i "${CMAKE_CURRENT_BINARY_DIR}/mynumpy.so" -o
    "${CMAKE_CURRENT_BINARY_DIR}/soldout/mynumpy.so" --section-headers
    --check-output
  DEPENDS sold mynumpy)

# ${CMAKE_CURRENT_BINARY_DIR}/test_soldout_modules_original.py is for debug.
configure_file(
  test_soldout_modules.py
  ${CMAKE_CURRENT_BINARY_DIR}/test_soldout_modules_original.py @ONLY)
configure_file(
  test_soldout_modules.py
  ${CMAKE_CURRENT_BINARY_DIR}/soldout/test_soldout_modules.py @ONLY)

add_test(
  NAME pybind_test
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/soldout"
  COMMAND pytest -v)
