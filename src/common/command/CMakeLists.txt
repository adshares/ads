SET (SOURCE
    ${SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/getaccount.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/setaccountkey.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/createnode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sendone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sendmany.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/createaccount.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/getaccounts.cpp
    PARENT_SCOPE
    )

SET (HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/errorcodes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pods.h
    ${CMAKE_CURRENT_SOURCE_DIR}/getaccount.h
    ${CMAKE_CURRENT_SOURCE_DIR}/setaccountkey.h
    ${CMAKE_CURRENT_SOURCE_DIR}/createnode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sendone.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sendmany.h
    ${CMAKE_CURRENT_SOURCE_DIR}/createaccount.h
    ${CMAKE_CURRENT_SOURCE_DIR}/getaccounts.h
    PARENT_SCOPE
    )

