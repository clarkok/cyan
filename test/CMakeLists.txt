add_subdirectory(googletest)

include_directories(${gtest_SOURCE_DIR}/include)
include_directories(${gtest_SOURCE_DIR})

add_definitions(-D__PROJECT_DIR__="${PROJECT_SOURCE_DIR}")

add_executable(test_all googletest/src/gtest-all.cc parser_test.cpp codegen_x64_test.cpp inliner_test.cpp dep_analyzer_test.cpp mem2reg_test.cpp loop_marker_test.cpp inst_rewriter_test.cpp phi_eliminator_test.cpp dead_code_eliminater_test.cpp unreachable_code_elimimater.cpp combined_test.cpp)
target_link_libraries(test_all gtest_main cyan)

