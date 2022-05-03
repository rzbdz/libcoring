# LOGGER
add_executable(do_logging do_logging.cpp)
target_link_libraries(do_logging logging ${CMAKE_THREAD_LIBS_INIT})
# use for simple apache benchmark test, perf, io_uring strategy, sqpoll and iou-wrk
add_executable(single_server single_server.cpp)
target_link_libraries(single_server uring logging)
TARGET_LINK_LIBRARIES(single_server)
# a real echo server, for rust-echo-bench
add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server uring logging ${CMAKE_THREAD_LIBS_INIT})

# some useless tests
add_executable(short_read_problem short_read_problem.cpp)
target_link_libraries(short_read_problem uring logging)
# show how to use timeout
add_executable(connect_with_timeout connect_with_timeout.cpp)
target_link_libraries(connect_with_timeout logging uring)

file(COPY public
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
target_link_libraries(http_server uring logging ${CMAKE_THREAD_LIBS_INIT})
# HTTP1.0 GET http server, for apache benchmark
add_executable(mhttp_server http_server.cpp)
target_link_libraries(mhttp_server logging uring ${CMAKE_THREAD_LIBS_INIT})
