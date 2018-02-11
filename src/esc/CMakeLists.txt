set(SRC_LST   
   main.cpp
   ../ed25519/ed25519.c
)

set(HEADER_FILES ../ed25519/ed25519.h)

add_executable(esc ${SRC_LST} ${HEADER_FILES})
target_link_libraries(esc LINK_PUBLIC common)
target_link_libraries(esc pthread)
target_link_libraries(esc ssl)
target_link_libraries(esc crypto)

# if you are building in-source, this is the same as CMAKE_SOURCE_DIR, otherwise
# this is the top level directory of your build tree
MESSAGE("CMAKE_BINARY_DIR:         " ${CMAKE_BINARY_DIR} )

# if you are building in-source, this is the same as CMAKE_CURRENT_SOURCE_DIR, otherwise this
# is the directory where the compiled or generated files from the current CMakeLists.txt will go to
MESSAGE("CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR} )

# this is the directory, from which cmake was started, i.e. the top level source directory
MESSAGE( "CMAKE_SOURCE_DIR:         " ${CMAKE_SOURCE_DIR} )

# this is the directory where the currently processed CMakeLists.txt is located in
MESSAGE( "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR} )

# contains the full path to the top level directory of your build tree
MESSAGE("PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR} )

MESSAGE("Binary2 Path: ", ${PROJECT_BINARY_DIR})

ADD_CUSTOM_COMMAND(TARGET esc
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy  esc ${PROJECT_BINARY_DIR}
)

