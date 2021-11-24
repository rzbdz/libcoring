# inherited from coring
set(FMT_SRCS
        core.h
        format.cc
        format.h
        format-inl.h)

# include file inherited from project coring
add_library(fmt ${FMT_SRCS})
