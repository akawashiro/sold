# 
# Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
# 
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto. Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
# 

find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

add_executable(sample_cublasLt_LtIgemmTensor
    main.cpp
    sample_cublasLt_LtIgemmTensor.cu
)

target_include_directories(sample_cublasLt_LtIgemmTensor PRIVATE
    # ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}
    ${TORCH_INCLUDE_DIRS}
)

set(CUDART_LIBRARY cudart)
set(CUBLASLT_LIBRARY cublasLt)

target_link_libraries(sample_cublasLt_LtIgemmTensor PRIVATE "${TORCH_LIBRARIES}" "${CUBLASLT_LIBRARY}" "${CUDART_LIBRARY}")

add_custom_target(
    sample_cublasLt_LtIgemmTensor_soldout ALL
  COMMAND
    GLOG_log_dir=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR}/sold -i
    ${CMAKE_CURRENT_BINARY_DIR}/sample_cublasLt_LtIgemmTensor -o
    ${CMAKE_CURRENT_BINARY_DIR}/sample_cublasLt_LtIgemmTensor.soldout
    --check-output
  DEPENDS sample_cublasLt_LtIgemmTensor sold
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(
    NAME cuBLASLt_LtIgemmTensor_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/sample_cublasLt_LtIgemmTensor.soldout
    )
