add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()

project(example-app)

find_package(Torch REQUIRED)

message(STATUS ${TORCH_CXX_FLAGS})
message(STATUS ${TORCH_LIBRARIES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

add_executable(example-app example-app.cpp)
target_link_libraries(example-app "${TORCH_LIBRARIES}")
# set_property(TARGET example-app PROPERTY CXX_STANDARD 14)

add_executable(c10-app c10-app.cc)
target_link_libraries(c10-app "${TORCH_LIBRARIES}" gtest_main gtest)
# set_property(TARGET c10-app PROPERTY CXX_STANDARD 14)

add_executable(c10-test c10-test.cc)
target_link_libraries(c10-test "${TORCH_LIBRARIES}" gtest_main gtest)

# add_executable(aten-app aten-app.cc)
# target_link_libraries(c10-app "${TORCH_LIBRARIES}")
# set_property(TARGET c10-app PROPERTY CXX_STANDARD 14)
