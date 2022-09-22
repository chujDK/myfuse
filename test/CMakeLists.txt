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

# block_device_test
add_executable(block_device_test block_device_test.cc)
target_link_libraries(block_device_test test_lib_base)
gtest_discover_tests(block_device_test)

# bcache_buf_test
add_executable(bcache_buf_test bcache_buf_test.cc)
target_link_libraries(bcache_buf_test test_lib_base)
gtest_discover_tests(bcache_buf_test)

#log_test
add_executable(log_test log_test.cc)
target_link_libraries(log_test PUBLIC test_lib_base)
gtest_discover_tests(log_test)

#inode_test
add_executable(inode_test inode_test.cc)
target_link_libraries(inode_test PUBLIC test_lib_base)
gtest_discover_tests(inode_test)
