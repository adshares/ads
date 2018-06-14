cmake_minimum_required(VERSION 2.8)

cmake_policy(SET CMP0015 NEW)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -m64 -fPIC -pthread")

if(UNIX AND NOT APPLE)
   add_definitions(-DLINUX)
   MESSAGE("..............LINUX SYSTEM..............")
endif()


if (NOT CMAKE_PROJECT_CONFIG)
    set(CMAKE_PROJECT_CONFIG "esc")
endif (NOT CMAKE_PROJECT_CONFIG)
set (CONFIG_FILE "default_${CMAKE_PROJECT_CONFIG}.h.in" )
project(${CMAKE_PROJECT_CONFIG})

if(EXISTS ${CONFIG_FILE} )
    message(FATAL_ERROR "Configuration file ${CONFIG_FILE} not exists")
    return()
endif(EXISTS ${CONFIG_FILE})
message("PROJECT CONFIG: " ${CMAKE_PROJECT_CONFIG} )
configure_file(${CONFIG_FILE} ${CMAKE_SOURCE_DIR}/common/default.h)

IF(CMAKE_BUILD_TYPE MATCHES "Debug")
add_definitions(-DDEBUG)
ENDIF(CMAKE_BUILD_TYPE MATCHES "Debug")


add_subdirectory(common)
add_subdirectory(esc)
add_subdirectory(escd)
add_subdirectory(tools)

IF(EXECUTE_LOCAL_TEST)
MESSAGE("..............EXECUTE_LOCAL_TEST:         " ${EXECUTE_LOCAL_TEST} )
install(FILES ../scripts/installesc.sh
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
    DESTINATION bin)
ENDIF()
