
#ifndef CORING_BUFFER_POOL_HPP
#define CORING_BUFFER_POOL_HPP
#include "coring/utils/buffer.hpp"
#include "coring/utils/io_utils.hpp"
#include "coring/utils/noncopyable.hpp"
namespace coring {
namespace detail {

struct provided_buffer_block {
  // I just abide the rules that
  // every block in the same group
  // have a same size.
  // an alternative is to use iovec...
  void *base_;
};

struct provided_buffer_group {
  __u16 group_id_;
  __u32 len_;  // clumped at 2 GB
  std::vector<provided_buffer_block> blocks_;
};

}  // namespace detail

class buffer_pool {
 public:
 private:
};
}  // namespace coring
#endif  // CORING_BUFFER_POOL_HPP
