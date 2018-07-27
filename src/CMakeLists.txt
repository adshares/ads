cmake_minimum_required(VERSION 2.8)

cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0023 NEW)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -m64 -fPIC -Os -fno-strict-aliasing")

#set(Boost_USE_STATIC_LIBS   ON)

if(UNIX AND NOT APPLE)
   add_definitions(-DLINUX)
   message("..............LINUX SYSTEM..............")
endif()

# Get version
if(NOT CMAKE_PROJECT_VERSION)
	set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version")
	if(EXISTS ${VERSION_FILE})
	    file(READ ${VERSION_FILE} CMAKE_PROJECT_VERSION)
	endif()
endif()

if(NOT CMAKE_PROJECT_VERSION)
    execute_process(
      COMMAND git describe --tags
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_PROJECT_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX REPLACE "^v(.*)$" "\\1" CMAKE_PROJECT_VERSION "${GIT_PROJECT_VERSION}")
    string(REGEX REPLACE "-" "+" CMAKE_PROJECT_VERSION "${CMAKE_PROJECT_VERSION}")
endif()

string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" PROJECT_VERSION "${CMAKE_PROJECT_VERSION}")

if(NOT CMAKE_PROJECT_CONFIG)
    set(CMAKE_PROJECT_CONFIG "ads")
endif(NOT CMAKE_PROJECT_CONFIG)

set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/default_${CMAKE_PROJECT_CONFIG}.h.in")
if(CMAKE_PROJECT_NAME)
	project(${CMAKE_PROJECT_NAME} VERSION ${PROJECT_VERSION})
else()
	project(${CMAKE_PROJECT_CONFIG} VERSION ${PROJECT_VERSION})
endif()

message("** PROJECT ${PROJECT_NAME} ${CMAKE_PROJECT_VERSION} : " ${CMAKE_BUILD_TYPE} " **")

if(NOT EXISTS ${CONFIG_FILE})
    message(FATAL_ERROR "Configuration file ${CONFIG_FILE} not exists")
    return()
endif()

message("** PROJECT CONFIG: " ${CMAKE_PROJECT_CONFIG} " **")
configure_file(${CONFIG_FILE} ${CMAKE_SOURCE_DIR}/common/default.h)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DDEBUG)
endif()
if(CMAKE_BUILD_TYPE MATCHES "Release")
    add_definitions(-DNDEBUG)
endif()

if(CODE_COVERAGE)
    message("** SOURCE CODE COVERAGE ENABLED **")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -g -O0 --coverage -fprofile-arcs -ftest-coverage ")
endif()

add_subdirectory(common)
add_subdirectory(esc)
add_subdirectory(escd)
add_subdirectory(tools)

if(${UNITTEST})
    message("\n\n***UNITTESTS ENABLED***\n\n\n")
    add_subdirectory(unittest)
endif()
