SET(LOG_SRCS
        async_logger.cpp
        logging.cpp
        log_file.cpp
        )
add_library(logging ${LOG_SRCS})
target_link_libraries(logging)