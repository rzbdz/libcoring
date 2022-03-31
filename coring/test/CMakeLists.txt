# inherited from coring
# file(GLOB TEST_SRCS "*.cpp")

# include file inherited from project coring
# add_library(coring_test ${TEST_SRCS})

# UIO
add_executable(io_context_test io_context_test.cpp)
target_link_libraries(io_context_test uring gtest gtest_main coring_utils fmt)

# LOGGER
add_executable(async_log_test async_logger_test.cpp)
target_link_libraries(async_log_test logging)

add_executable(onepage_webserver onepage_webserver.cpp)
target_link_libraries(onepage_webserver uring logging coring_net)
TARGET_LINK_LIBRARIES(onepage_webserver ${CMAKE_THREAD_LIBS_INIT})

add_executable(threaded_server revised_singlepage_webserver.cpp)
target_link_libraries(threaded_server uring logging coring_net)
TARGET_LINK_LIBRARIES(threaded_server ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_short_read test_short_read.cpp)
target_link_libraries(test_short_read uring logging coring_net)

# skiplist_map
add_executable(skiplist_test skiplist_test.cpp)
target_link_libraries(skiplist_test logging gtest gtest_main)

enable_testing()