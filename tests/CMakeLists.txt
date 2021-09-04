set(CMAKE_SKIP_RPATH ON)

add_library(test_base SHARED base.cc)

add_library(test_lib SHARED lib.cc)
target_link_libraries(test_lib PRIVATE test_base "-Wl,-R,'$$ORIGIN'")

add_executable(test_exe exe.cc)
target_link_libraries(test_exe test_lib "-Wl,-R,'$$ORIGIN'")

add_executable(test_hash exe.cc)
target_link_libraries(test_hash test_lib -Wl,--hash-style=sysv
                      "-Wl,-R,'$$ORIGIN'")
# 
# add_library(hoge_for_rename SHARED hoge_for_rename.c)
# set_target_properties(hoge_for_rename PROPERTIES SUFFIX "_original.so")
