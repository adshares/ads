
set(SOURCED
   office.hpp
   peer.hpp
   client.hpp   
   options.hpp
   server.hpp
   main.cpp
   ../../external/ed25519/ed25519.c
)

set(HEADERSD
)

include_directories("../../external")

add_subdirectory(commandhandler)
add_subdirectory(network)

add_executable(escd ${SOURCED} ${HEADERSD})
target_link_libraries(escd LINK_PUBLIC common)
target_link_libraries(escd pthread)
target_link_libraries(escd ssl)
target_link_libraries(escd crypto)

install(TARGETS escd RUNTIME DESTINATION bin)

