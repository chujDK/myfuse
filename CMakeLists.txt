cmake_minimum_required(VERSION 3.22)

project(myfuse)

set(CMAKE_C_FLAGS_DEBUG "-DVERBOSE -ggdb -Og -g3")
set(CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG)
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)

aux_source_directory(${CMAKE_SOURCE_DIR}/src/ DIR_SRC)
include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(mkfs)
