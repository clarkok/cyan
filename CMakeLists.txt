cmake_minimum_required(VERSION 3.4)
project(cyan)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O0 -std=c++11 -g -Wno-unused-variable -Wno-unused-parameter")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O0 -std=c++11 -g")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(third-party)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
