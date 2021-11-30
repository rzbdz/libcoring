# inherited from coring
set(UTILS_SRCS
        logging/async_logger.cpp
        logging/log_file.cpp
        codec.cpp
        thread_pool.cpp)

add_subdirectory(fmt)
# include file inherited from project coring
add_library(coring_utils ${UTILS_SRCS})
target_link_libraries(coring_utils fmt)