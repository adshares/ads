SET(SOURCE
    networkclient.cpp
    user.cpp
    logging.cpp
)

SET(HEADERS
    default.hpp
    servers.hpp
    hash.hpp
    message.hpp
    user.hpp
    networkclient.h
    logging.h
)

include_directories("../../external")

add_subdirectory(abstraction)
add_subdirectory(helper)
add_subdirectory(command)
add_subdirectory(parser)
add_subdirectory(utils)


add_library(common ${SOURCE} ${HEADERS})

find_package(Boost REQUIRED COMPONENTS thread system program_options filesystem iostreams)
include_directories(common ${Boost_INCLUDE_DIRS})
link_directories(common ${Boost_LIBRARY_DIRS})
target_link_libraries(common ${Boost_LIBRARIES})


target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
