SET (SRCS_LST
          networkclient.h
          networkclient.cpp
          default.hpp
          hlog.hpp
          servers.hpp
          hash.hpp
          message.hpp
          )


find_package(Boost REQUIRED COMPONENTS thread system program_options)

add_library(common ${SRCS_LST})
include_directories(common ${Boost_INCLUDE_DIRS})
link_directories(common ${Boost_LIBRARY_DIRS})
target_link_libraries(common ${Boost_LIBRARIES})


target_include_directories (common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
