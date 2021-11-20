# inherited from coring
file(GLOB PROACTOR_SRCS "*.cpp")

# include file inherited from project coring
add_library(coring_proactor ${PROACTOR_SRCS})
target_link_libraries(coring_proactor coring_utils)
