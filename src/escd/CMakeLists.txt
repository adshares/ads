set(DAEMON_NAME ${PROJECT_NAME}d)

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

add_executable(${DAEMON_NAME} ${SOURCED} ${HEADERSD})
target_link_libraries(${DAEMON_NAME} LINK_PUBLIC common)
target_link_libraries(${DAEMON_NAME} pthread)
target_link_libraries(${DAEMON_NAME} ssl)
target_link_libraries(${DAEMON_NAME} crypto)

install(TARGETS ${DAEMON_NAME} RUNTIME DESTINATION bin)

