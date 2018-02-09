cmake_minimum_required(VERSION 2.8)
project(esc)

set(CMAKE_CXX_STANDARD 14)

SET(CMAKE_CXX_FLAGS -pthread)


add_subdirectory(common)
add_subdirectory(esc)

#add_executable(esc ${SOURCE})
#target_link_libraries(esc LINK_PUBLIC common)
#add_subdirectory(escd)
#add_executable(esc "esc/main.cpp")
