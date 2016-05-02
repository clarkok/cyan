cmake_minimum_required(VERSION 3.4)
project(cyan)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall")

add_subdirectory(lib)
add_subdirectory(src)

