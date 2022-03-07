# inherited from coring
# file(GLOB TEST_SRCS "*.cpp")

# include file inherited from project coring
# add_library(coring_test ${TEST_SRCS})

# UIO
add_executable(uio_ctx_test uio_ctx_test.cpp)
target_link_libraries(uio_ctx_test uring gtest gtest_main coring_utils)

# LOGGER
add_executable(async_log_test async_log_test.cpp)
target_link_libraries(async_log_test logging)


enable_testing()