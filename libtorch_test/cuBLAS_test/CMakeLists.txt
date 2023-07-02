add_executable(cuBLAS_test cuBLAS_test.cc)
target_link_libraries(cuBLAS_test PRIVATE "${TORCH_LIBRARIES}")
target_include_directories(cuBLAS_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(
    cuBLAS_test_soldout ALL
  COMMAND
    GLOG_log_dir=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR}/sold -i
    ${CMAKE_CURRENT_BINARY_DIR}/cuBLAS_test -o
    ${CMAKE_CURRENT_BINARY_DIR}/cuBLAS_test.soldout --section-headers
    --check-output
  DEPENDS cuBLAS_test sold
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(
  NAME cuBLAS_test
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMAND ./cuBLAS_test.soldout)
