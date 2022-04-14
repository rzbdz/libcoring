/// This buffered is a wrapper to deal with the short-read/write
/// problem.
/// There should not be any short-read/write in regular files, so ,just separate them
#ifndef CORING_SOCKET_DUPLEXER_HPP
#define CORING_SOCKET_DUPLEXER_HPP
#include <coroutine>
#include <cstddef>
#include "socket.hpp"
#include "socket_reader.hpp"
#include "socket_writer.hpp"

namespace coring {

class socket_duplexer : public flex_socket_reader, public flex_socket_writer {
 public:
  explicit socket_duplexer(socket fd, int sz = buffer::default_size)
      : flex_socket_reader{fd, sz}, flex_socket_writer{fd, sz} {}
  explicit socket_duplexer(socket read_fd, socket write_fd, int sz = buffer::default_size)
      : flex_socket_reader{read_fd, sz}, flex_socket_writer{write_fd, sz} {}
  explicit socket_duplexer(socket read_fd, socket write_fd, int rdsz = buffer::default_size,
                           int wrsz = buffer::default_size)
      : flex_socket_reader{read_fd, rdsz}, flex_socket_writer{write_fd, wrsz} {}
};
}  // namespace coring

#endif  // CORING_SOCKET_DUPLEXER_HPP
