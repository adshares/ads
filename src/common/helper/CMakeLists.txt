SET (SOURCE
    ${SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/ascii.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hash.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/json.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/servers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hlog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/signatures.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vipkeys.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/block.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/serversheader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blocks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filewrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libarchive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blockfilereader.cpp
    PARENT_SCOPE
    )

SET (HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/ascii.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hash.h
    ${CMAKE_CURRENT_SOURCE_DIR}/socket.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/servers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hlog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/txsname.h
    ${CMAKE_CURRENT_SOURCE_DIR}/signatures.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vipkeys.h
    ${CMAKE_CURRENT_SOURCE_DIR}/block.h
    ${CMAKE_CURRENT_SOURCE_DIR}/serversheader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/blocks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/filewrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libarchive.h
    ${CMAKE_CURRENT_SOURCE_DIR}/blockfilereader.h
    PARENT_SCOPE
    )
