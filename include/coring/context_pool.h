// context_pool.h
// Created by PanJunzhong on 2022/4/29.
//

#ifndef CORING_CONTEXT_POOL_H
#define CORING_CONTEXT_POOL_H
#include <thread>
#include <list>
#include "io_context.hpp"
#include "coring/detail/io_context_service.hpp"
namespace coring {
namespace detail{
struct ctx_pool_worker{
  std::jthread thread;
  io_context* context;
};
}
class context_pool {
  std::list<std::jthread> threads_;
};
}  // namespace coring
#endif  // CORING_CONTEXT_POOL_H
