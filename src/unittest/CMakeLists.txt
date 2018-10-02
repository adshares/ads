set (UNITTEST_APP unittest)

SET (SRC_LIST
    unittest.cpp
    blocks_helper_test.cpp
    snapshot_test.cpp
    ../../external/ed25519/ed25519.c
    )

SET (HEADERS_LIST
    )

enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(${UNITTEST_APP} ${SRC_LIST})

target_link_libraries(${UNITTEST_APP} LINK_PUBLIC common gtest gtest_main common crypto)
link_directories(${UNITTEST_APP} ${GTEST_LIBRARY_DIRS})
