include_directories(${PROJECT_SOURCE_DIR}/lib)

set(PROJECT_SRCS main.cpp lib_functions.cpp)
add_executable(cyanc ${PROJECT_SRCS})

target_link_libraries(cyanc cyan)
