if (UNIX OR APPLE)
    SET (SOURCE
        ${SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/os_utils.cpp
        PARENT_SCOPE
        )
    SET (HEADERS
        ${HEADERS}
        ${CMAKE_CURRENT_SOURCE_DIR}/os_utils.h
        PARENT_SCOPE
        )
endif ()
