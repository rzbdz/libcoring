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

# datetime
add_executable(tcp_client_raw tcp_client_raw.cpp)
target_link_libraries(tcp_client_raw uring coring_utils)

add_executable(tcp_server_raw tcp_server_raw.cpp)
target_link_libraries(tcp_server_raw uring coring_utils)
# skiplist_map
add_executable(skiplist_test skiplist_test.cpp)
target_link_libraries(skiplist_test logging gtest gtest_main)

enable_testing()