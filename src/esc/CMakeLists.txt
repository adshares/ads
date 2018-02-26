set(SRC_LST   
   main.cpp
   responsehandler.cpp
   ../../external/ed25519/ed25519.c
)

set(HEADER_FILES
    responsehandler.h)

include_directories("../../external")

add_executable(esc ${SRC_LST} ${HEADER_FILES})

target_link_libraries(esc LINK_PUBLIC common)
target_link_libraries(esc pthread)
target_link_libraries(esc ssl)
target_link_libraries(esc crypto)

install (TARGETS esc RUNTIME DESTINATION bin)

# if you are building in-source, this is the same as CMAKE_SOURCE_DIR, otherwise
# this is the top level directory of your build tree
MESSAGE("CMAKE_BINARY_DIR:         " ${CMAKE_BINARY_DIR} )

