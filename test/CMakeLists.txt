set(CMAKE_CXX_STANDARD 17)
find_package(GTest REQUIRED)
include_directories(PUBLIC ${GTEST_INCLUDE_DIRS})

add_custom_command(OUTPUT disk.img
COMMAND dd if=/dev/zero of=disk.img bs=512 count=400000)
add_custom_target(BuildDisk ALL DEPENDS disk.img)

# block_device_test
add_executable(block_device_test block_device_test.cc)
target_link_libraries(block_device_test PUBLIC ${GTEST_BOTH_LIBRARIES} PUBLIC ${FUSE3_LIBRARIES} myfuse_static_base)
gtest_discover_tests(block_device_test)

# bcache_buf_test
add_executable(bcache_buf_test bcache_buf_test.cc)
target_link_libraries(bcache_buf_test PUBLIC ${GTEST_BOTH_LIBRARIES} PUBLIC ${FUSE3_LIBRARIES} myfuse_static_base)
gtest_discover_tests(bcache_buf_test)

