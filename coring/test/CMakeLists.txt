# inherited from coring
# file(GLOB TEST_SRCS "*.cpp")

# include file inherited from project coring
# add_library(coring_test ${TEST_SRCS})

### GTest
# io_context, timer
add_executable(io_context_test io_context_test.cpp)
target_link_libraries(io_context_test uring gtest gtest_main coring_utils fmt ${CMAKE_THREAD_LIBS_INIT})
# skiplist_map
add_executable(skiplist_test skiplist_test.cpp)
target_link_libraries(skiplist_test logging gtest gtest_main ${CMAKE_THREAD_LIBS_INIT})
# For github Actions.
add_executable(
        unit_tests
        io_context_test.cpp
        skiplist_test.cpp
)
target_link_libraries(
        unit_tests
        PRIVATE
        gtest_main
        uring gtest gtest_main logging coring_utils fmt ${CMAKE_THREAD_LIBS_INIT}
)
### Non-GTest
# LOGGER
add_executable(async_log_test async_logger_test.cpp)
target_link_libraries(async_log_test logging)
# simple usage/demo/tests
add_executable(onepage_webserver onepage_webserver.cpp)
target_link_libraries(onepage_webserver uring logging coring_net)
TARGET_LINK_LIBRARIES(onepage_webserver ${CMAKE_THREAD_LIBS_INIT})
# simple usage/demo/tests
add_executable(threaded_server revised_singlepage_webserver.cpp)
target_link_libraries(threaded_server uring logging coring_net)
TARGET_LINK_LIBRARIES(threaded_server ${CMAKE_THREAD_LIBS_INIT})
# simple usage/demo/tests
add_executable(test_short_read test_short_read.cpp)
target_link_libraries(test_short_read uring logging coring_net)
# simple usage/demo/tests
add_executable(test_timeout_connect test_timeout_connect.cpp)
target_link_libraries(test_timeout_connect uring coring_net)

enable_testing()