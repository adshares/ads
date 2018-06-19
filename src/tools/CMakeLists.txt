set (MSG_PARSER_APP msg_printer)

include_directories("../../external")

SET (SRC_LIST
    main.cpp
    dataprinter.cpp
    msgprinter.cpp
    srvprinter.cpp
    msglistprinter.cpp
    hdrprinter.cpp
    undoprinter.cpp
    hlogprinter.cpp
    signaturesprinter.cpp
    datprinter.cpp
    vipprinter.cpp
    inxprinter.cpp
    logprinter.cpp
    ../../external/ed25519/ed25519.c
    )

SET (HEADERS_LIST
    dataprinter.h
    msgprinter.h
    srvprinter.h
    msglistprinter.h
    hdrprinter.h
    undoprinter.h
    hlogprinter.h
    signaturesprinter.h
    datprinter.h
    vipprinter.h
    inxprinter.h
    logprinter.h
    )


add_executable(${MSG_PARSER_APP} ${SRC_LIST})

target_link_libraries(${MSG_PARSER_APP} LINK_PUBLIC common)
target_link_libraries(${MSG_PARSER_APP} PRIVATE ssl crypto)
