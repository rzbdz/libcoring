# LOGGER
add_executable(do_logging do_logging.cpp)
target_link_libraries(do_logging logging ${CMAKE_THREAD_LIBS_INIT})
# a real echo server, for rust-echo-bench
add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server uring logging ${CMAKE_THREAD_LIBS_INIT})

# show how to use timeout
add_executable(connect_with_timeout connect_with_timeout.cpp)
target_link_libraries(connect_with_timeout logging uring)

file(COPY public
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
# HTTP1.0 GET http server, for apache benchmark
add_executable(mhttp_server http_server.cpp)
target_link_libraries(mhttp_server logging uring ${CMAKE_THREAD_LIBS_INIT})
