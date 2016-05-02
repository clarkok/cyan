set(PROJECT_SRCS main.cpp)
add_executable(cyanc ${PROJECT_SRCS})

target_link_libraries(cyanc cyan)
