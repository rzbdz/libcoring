# inherited from coring
set(UTILS_SRCS
        codec.cpp
        thread_pool.cpp
        thread.cpp)

# include file inherited from project coring
add_library(coring_utils ${UTILS_SRCS})