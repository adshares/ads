set (MSG_PARSER_APP msg_printer)

include_directories("../../external")

SET (SRC_LIST
    main.cpp
    jsonprinter.cpp
    ../../external/ed25519/ed25519.c
    )

SET (HEADERS_LIST
    jsonprinter.h
    )


add_executable(${MSG_PARSER_APP} ${SRC_LIST})

target_link_libraries(${MSG_PARSER_APP} LINK_PUBLIC common)
target_link_libraries(${MSG_PARSER_APP} ssl crypto)
