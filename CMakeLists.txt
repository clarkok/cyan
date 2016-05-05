cmake_minimum_required(VERSION 3.4)
project(cyan)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "CLANG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -std=c++11 -g -Wall")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -std=c++11 -g -Wall")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(third-party)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
