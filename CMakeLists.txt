cmake_minimum_required(VERSION 3.22)

project(myfuse)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

set(CMAKE_C_FLAGS_DEBUG "-DVERBOSE -ggdb -Og -g3")
set(CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG)
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)

set(DIR_SRC "${CMAKE_SOURCE_DIR}/src")
aux_source_directory(${CMAKE_SOURCE_DIR}/src/ ALL_SRC)
list(REMOVE_ITEM ALL_SRC "src/main.c")
include_directories(${CMAKE_SOURCE_DIR}/include)
add_library(myfuse_static_base STATIC ${ALL_SRC})
add_executable(myfuse ${DIR_SRC}/main.c)
target_include_directories(myfuse PUBLIC ${FUSE3_INCLUDE_DIRS})
target_include_directories(myfuse_static_base PUBLIC ${FUSE3_INCLUDE_DIRS})
target_link_libraries(myfuse PUBLIC ${FUSE3_LIBRARIES} myfuse_static_base)

add_subdirectory(mkfs)
