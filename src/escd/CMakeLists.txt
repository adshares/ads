
set(SRC_LST2
   office.hpp
   peer.hpp
   client.hpp
   message.hpp
   options.hpp
   server.hpp
   main.cpp
   ../ed25519/ed25519.c
)

set(HEADER_FILES2 ../ed25519/ed25519.h)

add_executable(escd ${SRC_LST2} ${HEADER_FILES2})
target_link_libraries(escd LINK_PUBLIC common)
target_link_libraries(escd pthread)
target_link_libraries(escd ssl)
target_link_libraries(escd crypto)


ADD_CUSTOM_COMMAND(TARGET esc
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy  escd ${PROJECT_BINARY_DIR}
)
