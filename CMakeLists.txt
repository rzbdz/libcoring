cmake_minimum_required(VERSION 3.16)
project(coring C CXX)

set(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -std=c++20")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -std=c++20")


# PROJECT_SOURCE_DIR is the directory of current cmake path
include_directories(${PROJECT_SOURCE_DIR})

add_subdirectory(coring/utils)
add_subdirectory(coring/net)
add_subdirectory(coring/async)
add_subdirectory(coring/io)