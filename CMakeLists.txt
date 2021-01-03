cmake_minimum_required(VERSION 3.0)
project(sold LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(third-party)
get_third_party(glog)
set(WITH_GFLAGS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/glog ${CMAKE_CURRENT_BINARY_DIR}/glog EXCLUDE_FROM_ALL)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/glog ${CMAKE_CURRENT_BINARY_DIR}/glog/src)
add_definitions(-DC10_USE_GLOG=1)

add_library(
    sold_lib
    sold.cc
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
    sold_main.cc
    )
target_link_libraries(sold sold_lib glog)

add_executable(
    print_dtrela
    print_dtrela.cc
    )
target_link_libraries(print_dtrela sold_lib glog)

add_executable(
    print_dynsymtab
    print_dynsymtab.cc
    )
target_link_libraries(print_dynsymtab sold_lib glog)

add_executable(
    print_tls
    print_tls.cc
    )
target_link_libraries(print_tls sold_lib glog)

add_executable(
    print_version
    print_version.cc
    )
target_link_libraries(print_version sold_lib glog)

add_executable(
    print_ehframe
    print_ehframe.cc
    )
target_link_libraries(print_ehframe sold_lib glog)

add_subdirectory(tests)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake ${CMAKE_CURRENT_BINARY_DIR})
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
