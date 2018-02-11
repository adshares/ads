set(SRC_LST   
   settings.hpp
   user.cpp
   main.cpp
   ../ed25519/ed25519.c
)

set(HEADER_FILES ../ed25519/ed25519.h)

add_executable(esc ${SRC_LST} ${HEADER_FILES})
target_link_libraries(esc LINK_PUBLIC common)
target_link_libraries(esc pthread)
target_link_libraries(esc ssl)
target_link_libraries(esc crypto)

