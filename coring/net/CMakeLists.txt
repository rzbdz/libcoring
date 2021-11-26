# inherited from coring
file(GLOB NET_SRCS "*.cpp")

# include file inherited from project coring
add_library(coring_net ${NET_SRCS})
target_link_libraries(coring_net coring_utils)