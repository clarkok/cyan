add_subdirectory(googletest)

include_directories(${gtest_SOURCE_DIR}/include)
include_directories(${gtest_SOURCE_DIR})

add_executable(test_all googletest/src/gtest-all.cc parser_test.cpp)
target_link_libraries(test_all gtest_main cyan)

