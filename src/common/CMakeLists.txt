SET(SRCS_LST
    networkclient.cpp
    user.cpp
)

SET(HDR_LST
    default.hpp
    hlog.hpp
    servers.hpp
    hash.hpp
    message.hpp
    user.hpp
    networkclient.h
)

add_subdirectory(abstraction)
#add_subdirectory(command)


add_library(common ${SRCS_LST} ${HDR_LST})

find_package(Boost REQUIRED COMPONENTS thread system program_options)
include_directories(common ${Boost_INCLUDE_DIRS})
link_directories(common ${Boost_LIBRARY_DIRS})
target_link_libraries(common ${Boost_LIBRARIES})


target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
