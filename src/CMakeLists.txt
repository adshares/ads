cmake_minimum_required(VERSION 2.8)
project(esc)

cmake_policy(SET CMP0015 NEW)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall ")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG}")

# set the final binary dir
set(PROJECT_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)

IF(CMAKE_BUILD_TYPE MATCHES "Debug")
add_definitions(-DDEBUG)
ENDIF(CMAKE_BUILD_TYPE MATCHES "Debug")

SET(CMAKE_CXX_FLAGS -pthread)

add_subdirectory(common)
add_subdirectory(esc)
add_subdirectory(escd)

install(FILES ../scripts/installesc.sh
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
    DESTINATION bin)
