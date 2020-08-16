cmake_minimum_required(VERSION 3.0)
project(sold LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

add_library(
  sold_lib
  elf_binary.cc
  hash.cc
  ldsoconf.cc
  strtab_builder.cc
  symtab_builder.cc
  shdr_builder.cc
  utils.cc
  version_builder.cc
  )

add_executable(
  sold
  sold.cc
  )
target_link_libraries(sold sold_lib)

add_executable(
  print_dtrela
  print_dtrela.cc
  )
target_link_libraries(print_dtrela sold_lib)

add_executable(
  print_dynsymtab
  print_dynsymtab.cc
  )
target_link_libraries(print_dynsymtab sold_lib)

add_subdirectory(tests)

foreach(t exe hash)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out"
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/sold" "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}" -o "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out"
    DEPENDS sold test_${t}
    )

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out_stamp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out"
    )
  add_custom_target(run_test_${t}_out ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out_stamp)
endforeach()
