cmake_minimum_required(VERSION 3.0)
project(sold LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

add_executable(
  sold
  sold.cc
  elf_binary.cc
  hash.cc
  ldsoconf.cc
  utils.cc
  )

add_executable(
  print_dynsymtab
  print_dynsymtab.cc
  elf_binary.cc
  hash.cc
  ldsoconf.cc
  utils.cc
  )

add_subdirectory(tests)

foreach(t exe hash)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out"
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/sold" "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}" "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out"
    DEPENDS sold test_${t}
    )

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out_stamp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out"
    )
  add_custom_target(run_test_${t}_out ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out_stamp)
endforeach()
