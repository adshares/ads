SET(SOURCE
    networkclient.cpp
    user.cpp
)

SET(HEADERS
    default.hpp
    hlog.hpp
    servers.hpp
    hash.hpp
    message.hpp
    user.hpp
    networkclient.h
)

include_directories("../../external")

add_subdirectory(abstraction)
add_subdirectory(helper)
add_subdirectory(command)


add_library(common ${SOURCE} ${HEADERS})

find_package(Boost REQUIRED COMPONENTS thread system program_options)
include_directories(common ${Boost_INCLUDE_DIRS})
link_directories(common ${Boost_LIBRARY_DIRS})
target_link_libraries(common ${Boost_LIBRARIES})


target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
