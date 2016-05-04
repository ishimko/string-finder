cmake_minimum_required(VERSION 3.5)
project(string_finder)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.c)
add_executable(string_finder ${SOURCE_FILES})
target_link_libraries(string_finder pthread)