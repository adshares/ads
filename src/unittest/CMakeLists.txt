set (UNITTEST_APP unittest)

SET (SRC_LIST
    unittest.cpp
    compressor_test.cpp
    blocks_helper_test.cpp
    )

SET (HEADERS_LIST
    )

enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(${UNITTEST_APP} ${SRC_LIST})

target_link_libraries(${UNITTEST_APP} LINK_PUBLIC common)
link_directories(${UNITTEST_APP} ${GTEST_LIBRARY_DIRS})
target_link_libraries(${UNITTEST_APP} gtest gtest_main)
