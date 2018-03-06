SET (SOURCED
    ${SOURCED}
    ${CMAKE_CURRENT_SOURCE_DIR}/commandservice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/commandhandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/getaccounthandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/setaccountkeyhandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/createnodehandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sendonehandler.cpp
    PARENT_SCOPE
    )

SET (HEADERSD
    ${HEADERSD}
    ${CMAKE_CURRENT_SOURCE_DIR}/commandservice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/commandhandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/getaccounthandler.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/setaccountkeyhandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/createnodehandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sendonehandler.h
    PARENT_SCOPE
    )


