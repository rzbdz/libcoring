# inherited from coring
# file(GLOB TEST_SRCS "*.cpp")

# include file inherited from project coring
# add_library(coring_test ${TEST_SRCS})

### GTest
# io_context, timer
add_executable(io_context_test io_context_test.cpp)
target_link_libraries(io_context_test uring gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
# skiplist_map
add_executable(skiplist_test skiplist_test.cpp)
target_link_libraries(skiplist_test logging gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
#buffer
add_executable(buffer_test buffer_test.cpp)
target_link_libraries(buffer_test gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
#buffer selection
add_executable(buffer_selection_test buffer_selection_test.cpp)
target_link_libraries(buffer_selection_test gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
# For github Actions.
add_executable(
        unit_tests
        io_context_test.cpp
        skiplist_test.cpp
        buffer_test.cpp
        buffer_selection_test.cpp
)
target_link_libraries(
        unit_tests
        PRIVATE
        gtest_main
        uring gtest gtest_main logging ${CMAKE_THREAD_LIBS_INIT}
)
enable_testing()