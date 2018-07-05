SET (SOURCE
    ${SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tokendb.cpp
    PARENT_SCOPE
    )

SET (HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/tokendb.h
    PARENT_SCOPE
    )

##lmdb library##
find_path(LMDB_INCLUDE_DIR NAMES lmdb.h)
find_library(LMDB_LIBRARY NAMES lmdb)
set(LMDB_LIBRARIES ${LMDB_LIBRARY})
set(LMDB_INCLUDE_DIRS ${LMDB_INCLUDE_DIR})
####
