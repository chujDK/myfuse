set(CMAKE_CXX_STANDARD 17)
find_package(GTest REQUIRED)
include_directories(PUBLIC ${GTEST_INCLUDE_DIRS})

add_custom_command(OUTPUT disk.img
COMMAND dd if=/dev/zero of=disk.img bs=512 count=400000)
add_custom_target(BuildDisk ALL DEPENDS disk.img)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_library(test_lib_base STATIC test_util.cc)
target_link_libraries(test_lib_base PUBLIC ${GTEST_BOTH_LIBRARIES} PUBLIC ${FUSE3_LIBRARIES} myfuse_static_base)
target_include_directories(test_lib_base PUBLIC ${FUSE3_INCLUDE_DIRS})

macro(gtest_target target)
  add_executable(${target}_test ${target}_test.cc)
  target_link_libraries(${target}_test test_lib_base)
  gtest_discover_tests(${target}_test)

  if (CMAKE_BUILD_TYPE MATCHES Debug)
    if (CodeCoverage MATCHES ON)
      include(CodeCoverage)
      setup_target_for_coverage(${target}_test_coverage ${target}_test ${target}_coverage)
      setup_target_for_coverage_cobertura(${target}_test_cobertura ${target}_test ${target}_coverage --gtest_output=xml:coverage.junit.xml)
    endif()
  endif()
endmacro()

gtest_target(block_device)
gtest_target(bcache_buf)
gtest_target(log)
gtest_target(inode)
gtest_target(block_allocator)
