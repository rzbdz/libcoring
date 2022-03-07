SET(LOG_SRCS
        async_logger.cpp
        logging.cpp
        )
add_library(logging ${LOG_SRCS})
add_subdirectory(fmt)
target_link_libraries(logging fmt)