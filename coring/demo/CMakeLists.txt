# LOGGER
add_executable(do_logging do_logging.cpp)
target_link_libraries(do_logging logging ${CMAKE_THREAD_LIBS_INIT})
# simple usage/demo/tests
add_executable(single_server ../demo/single_server.cpp)
target_link_libraries(single_server uring logging coring_net)
TARGET_LINK_LIBRARIES(single_server)
# simple usage/demo/tests
add_executable(threaded_server ../demo/threaded_server.cpp)
target_link_libraries(threaded_server uring logging coring_net)
TARGET_LINK_LIBRARIES(threaded_server ${CMAKE_THREAD_LIBS_INIT})
# simple usage/demo/tests
add_executable(echo_server ../demo/echo_server.cpp)
target_link_libraries(echo_server uring logging coring_net ${CMAKE_THREAD_LIBS_INIT})

# simple usage/demo/tests
add_executable(short_read_problem short_read_problem.cpp)
target_link_libraries(short_read_problem uring logging coring_net)
# simple usage/demo/tests
add_executable(connect_with_timeout connect_with_timeout.cpp)
target_link_libraries(connect_with_timeout uring coring_net)