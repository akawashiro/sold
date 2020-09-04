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

add_executable(
  print_tls
  print_tls.cc
  )
target_link_libraries(print_tls sold_lib)

add_executable(
  print_verneed
  print_verneed.cc
  )
target_link_libraries(print_verneed sold_lib)

add_subdirectory(tests)

include(CTest)

if(BUILD_TESTING)
  foreach(t exe hash)
    add_custom_target(
      test_${t}_out ALL
      COMMAND "${CMAKE_CURRENT_BINARY_DIR}/sold" "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}" -o "${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out"  --section-headers
      DEPENDS sold test_${t}
      )

    add_test(
      NAME ${t}_test
      COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests/test_${t}_out
      )
  endforeach()
endif()
