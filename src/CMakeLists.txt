cmake_minimum_required(VERSION 2.8)
project(esc)

set(CMAKE_CXX_STANDARD 14)

SET(CMAKE_CXX_FLAGS -pthread)

# set the final binary dir
set(PROJECT_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)


add_subdirectory(common)
add_subdirectory(esc)
add_subdirectory(escd)
