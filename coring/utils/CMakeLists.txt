# inherited from coring
set(UTILS_SRCS
        logging/logger.cpp)

# include file inherited from project coring
add_library(coring_utils ${UTILS_SRCS})
target_link_libraries(coring_utils pthread)
