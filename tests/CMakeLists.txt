set(CMAKE_SKIP_RPATH ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(test_base SHARED base.cc)

add_library(test_lib SHARED lib.cc)
target_link_libraries(
  test_lib
  PRIVATE
  test_base
  "-Wl,-R,'$$ORIGIN'"
  )

add_executable(test_exe exe.cc)
target_link_libraries(
  test_exe
  test_lib
  "-pie -fPIE -Wl,-R,'$$ORIGIN'"
  )

add_executable(test_hash exe.cc)
target_link_libraries(
  test_hash
  test_lib
  -Wl,--hash-style=sysv
  "-pie -fPIE -Wl,-R,'$$ORIGIN'"
  )
