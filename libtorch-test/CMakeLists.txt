cmake_minimum_required(VERSION 3.4)
project(torch-test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

# Delete a symlink made by the user
add_custom_target(remove_torch_test
    COMMAND rm -f libtorch_test.so
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_library(torch_test SHARED torch_test.cc)
target_link_libraries(torch_test PRIVATE "${TORCH_LIBRARIES}")
add_dependencies(torch_test remove_torch_test)

add_custom_target(torch_test_original ALL
    COMMAND mv libtorch_test.so libtorch_test.so.original 
    DEPENDS torch_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(torch_test_soldout ALL
    COMMAND GLOG_log_dir=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR}/sold -i ${CMAKE_CURRENT_BINARY_DIR}/libtorch_test.so.original -o ${CMAKE_CURRENT_BINARY_DIR}/libtorch_test.so.soldout --section-headers --check-output
    DEPENDS torch_test_original sold
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(torch_test_soldout_wo_section ALL
    COMMAND GLOG_log_dir=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR}/sold -i ${CMAKE_CURRENT_BINARY_DIR}/libtorch_test.so.original -o ${CMAKE_CURRENT_BINARY_DIR}/libtorch_test.so.wosection.soldout --check-output
    DEPENDS torch_test_original sold
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(torch_test_main ALL
    COMMAND g++ ${CMAKE_CURRENT_SOURCE_DIR}/torch_test_main.cc ${CMAKE_CURRENT_BINARY_DIR}/libtorch_test.so.original -o torch_test_main
    DEPENDS torch_test_original
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(torch_test_main_linked_soldout ALL
    COMMAND g++ ${CMAKE_CURRENT_SOURCE_DIR}/torch_test_main.cc ${CMAKE_CURRENT_BINARY_DIR}/libtorch_test.so.soldout -o torch_test_main_linked_sold
    DEPENDS torch_test_soldout
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
