
#ifndef CORING_BUFFER_POOL_HPP
#define CORING_BUFFER_POOL_HPP
#include <list>
#include <functional>

#ifndef NO_IO_CONTEXT
#include "coring/io/io_context.hpp"
#endif
#include "coring/utils/buffer.hpp"
#include "coring/utils/io_utils.hpp"
#include "coring/utils/noncopyable.hpp"
namespace coring::detail {
struct buffer_id_t {
  buffer_id_t(const char (&char2)[2]) {  // NOLINT
    // e.g.: "AB" => 0x4142 / 0x4241
    val_ = *reinterpret_cast<const __u16 *>(char2);
  }
  buffer_id_t(const char *char2) {  // NOLINT
    // e.g.: "AB" => 0x4142 / 0x4241
    val_ = *reinterpret_cast<const __u16 *>(char2);
  }
  explicit buffer_id_t() : val_{0} {}
  buffer_id_t(__u16 v) : val_{v} {}  // NOLINT
  buffer_id_t &operator=(__u16 val) {
    val_ = val;
    return *this;
  }
  __u16 val_;
  operator __u16() const { return val_; }  // NOLINT
};
}  // namespace coring::detail
namespace std {
template <>
struct hash<coring::detail::buffer_id_t> {
 public:
  std::size_t operator()(const coring::detail::buffer_id_t &b) const {
    return std::hash<__u16>()(static_cast<__u16>(b));
  }
};
}  // namespace std
namespace coring {
#ifndef NO_IO_CONTEXT
template <typename ContextService = coro>
#else
template <typename ContextService>
#endif
class selected_buffer_resource;

template <typename ContextService>
class buffer_pool_base;

#ifndef NO_IO_CONTEXT
typedef buffer_pool_base<coro> buffer_pool;
#endif

class selected_buffer final : public fixed_buffer {
 public:
  selected_buffer(char *s, size_t len, detail::buffer_id_t group_id, __u16 buf_id)
      : fixed_buffer(s, len), group_id_(group_id), buf_id_(buf_id) {}

  template <int N>
  selected_buffer(char (&s)[N], detail::buffer_id_t group_id, __u16 &buf_id)
      : fixed_buffer(s), group_id_(group_id), buf_id_(buf_id) {}

 private:
  template <class U>
  friend class buffer_pool_base;
  template <class V>
  friend class selected_buffer_resource;

 public:
  __u16 buffer_id() { return buf_id_; }

 private:
  detail::buffer_id_t group_id_;
  __u16 buf_id_;
};
template <typename ContextService>
class selected_buffer_resource : noncopyable {
 public:
  explicit selected_buffer_resource(selected_buffer &val) : val(&val) {}
  selected_buffer_resource(selected_buffer_resource &&rhs) noexcept : val(rhs.val) { rhs.val = nullptr; }
  ~selected_buffer_resource() {
    /// FIXME: if the return value is <=0, we have big problem...
    /// TODO: I prefer TODO...
    /// You may want to have a look on: P1662 Adding async RAII support to coroutines, FYI:
    /// @see https://github.com/cplusplus/papers/issues?q=RAII
    if (val != nullptr) {
      [[maybe_unused]] auto ret = ContextService::get_io_context_ref().provide_buffers(
          const_cast<char *>(val->data()), static_cast<int>(val->size()), 1, val->group_id_, val->buf_id_);
      val->clear();
    }
  }
  selected_buffer &get() { return *val; }

 private:
  selected_buffer *val;
};

namespace detail {
class provided_buffer_group {
 public:
  detail::buffer_id_t group_id{};
  detail::buffer_id_t start_buf_id{};
  int nbytes_per_block{0};  // clumped at 2 GB
  std::vector<selected_buffer> blocks{};
};
}  // namespace detail

struct buffer_group_iterator : std::vector<detail::provided_buffer_group>::const_iterator {};

struct buffer_block_iterator : std::vector<char *>::const_iterator {};

/// Only support reading from a file descriptor.
template <typename ContextService>
class buffer_pool_base {
  typedef detail::provided_buffer_group group_t;

 public:
  typedef detail::buffer_id_t id_t;
  buffer_pool_base() {}

  void return_back(selected_buffer &val) {
    val.clear();
    [[maybe_unused]] auto ret = ContextService::get_io_context_ref().provide_buffers(
        const_cast<char *>(val.data()), static_cast<int>(val.size()), 1, val.group_id_, val.buf_id_);
  }

  task<int> returned_back(selected_buffer &val) {
    val.clear();
    co_return co_await ContextService::get_io_context_ref().provide_buffers(
        const_cast<char *>(val.data()), static_cast<int>(val.size()), 1, val.group_id_, val.buf_id_);
  }

 private:
  group_t &get_or_create_group_by_id(id_t gid) {
    auto it = map_.find(gid);
    if (it != map_.end()) {
      return it->second;
    } else {
      return map_[gid];
    }
  }

 public:
  //  void print(group_t &g_ref) {
  //    for (auto &a : g_ref.blocks) {
  //      std::cout << "a.id: " << a.buf_id_ << ", a.sz: " << a.size() << ", a.writable" << a.writable() << "\n";
  //    }
  //  }
  task<> provide_group_contiguous(char *base, __u16 nbytes_per_block, int how_many_blocks, id_t g_name) {
    auto ret = co_await ContextService::get_io_context_ref().provide_buffers(base, nbytes_per_block, how_many_blocks,
                                                                             g_name, 0);
    if (ret < 0) {
      throw std::system_error(std::error_code{-ret, std::system_category()});
    }
    group_t &g_ref = get_or_create_group_by_id(g_name);
    g_ref.group_id = g_name;
    g_ref.nbytes_per_block = nbytes_per_block;
    __u16 i = 0;
    g_ref.blocks.reserve(10);
    for (char *cur = base; cur < base + (nbytes_per_block * how_many_blocks); cur += nbytes_per_block, i++) {
      // LDR("emplace one");
      g_ref.blocks.emplace_back(selected_buffer{cur, nbytes_per_block, g_name, i});
      // LDR("emplaced: sz: %lu", g_ref.blocks.back().size());
    }
  }

 private:
  inline auto find_group(id_t g_name) {
    auto it = map_.find(g_name);
    if (it == map_.end()) {
      throw std::runtime_error("wrong group id for buffer selection");
    }
    return it;
  }

 public:
  /// RAII
  /// \param fd
  /// \param g_name
  /// \param nbytes
  /// \return
  task<selected_buffer &> try_read_block(int fd, id_t g_name, off_t offset = 0) {
    auto it = find_group(g_name);
    int nbytes = it->second.nbytes_per_block;
    // LOG_TRACE("co await read buffer_select");
    auto ret = co_await ContextService::get_io_context_ref().read_buffer_select(fd, g_name, nbytes, offset);
    auto res = ret.first, flag = ret.second;
    if (res <= 0) {
      throw std::system_error(std::error_code{-res, std::system_category()});
    }
    auto &g = it->second;
    g.blocks[flag].push_back(res);
    co_return g.blocks[flag];
  }

 private:
  std::unordered_map<detail::buffer_id_t, group_t> map_{};
};
}  // namespace coring
#endif  // CORING_BUFFER_POOL_HPP
