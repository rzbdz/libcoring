cmake_minimum_required(VERSION 3.16)
project(coring C CXX)
FIND_PACKAGE(Threads)
# please use TARGET_LINK_LIBRARIES(XXX ${CMAKE_THREAD_LIBS_INIT}) when you need pthread.
set(CMAKE_CXX_STANDARD 20)
# use —DCMAKE_BUILD_TYPE=DEBUG
# —DCMAKE_BUILD_TYPE=RELEASE
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -std=c++20 -pthread")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 -Wall -std=c++20 -pthread")

# PROJECT_SOURCE_DIR is the directory of current cmake path
include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(source/logging)

add_subdirectory(test)
add_subdirectory(examples)