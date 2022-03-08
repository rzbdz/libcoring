
#ifndef CORING_UIO_HPP
#define CORING_UIO_HPP
#pragma once
#include <functional>
#include <system_error>
#include <chrono>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <liburing.h>
#ifndef NDEBUG
#include <execinfo.h>
#endif

#include "coring/async/task.hpp"
#include "coring/utils/io_utils.hpp"
#include "coring/async/uio_async.hpp"

namespace coring::detail {
class uio {
  // This class is adopted from project liburing4cpp (MIT license).
  //
  // Since io_uring is still under development
  // iteratively, early version of related library should
  // be compatible with any possible old version of kernel and future updates,
  // But I do want to use io_uring always with the latest version especially
  // for performance reason.
  //
  // why I make another wrapper over it here is because the funtion
  // signature in both liburing and liburing4cpp is
  // designed in C style passed by pointer, which will be
  // a potential danger comparing the simple safety of smart ptr
  // in modern cpp.
  //
  // I did make little modification to it to adapt socket IO usage, but
  // I think the most important job is to couple the uio and library buffer.
  // also the async interface is modified modeling after boost::asio io_context.
  // it's not very 'proactor', but further design of interface is need to
  // be taken place.
 public:
  /** Init uio / io_uring object
   * @see io_uring_setup(2)
   * @param entries Maximum sqe can be gotten without submitting
   * @param flags flags used to init io_uring
   * @param wq_fd existing io_uring ring_fd used by IORING_SETUP_ATTACH_WQ
   * @note uio is NOT thread safe, nor is liburing. When used in a
   *       multi-threaded program, it's highly recommended to create
   *       uio/io_uring instance per thread, and set
   * IORING_SETUP_ATTACH_WQ flag to make sure that kernel shares the only async
   * worker thread pool. See `IORING_SETUP_ATTACH_WQ` for detail.
   */
  uio(int entries = 64, uint32_t flags = 0, uint32_t wq_fd = 0) {
    io_uring_params p = {
        .flags = flags,
        .wq_fd = wq_fd,
    };
    io_uring_queue_init_params(entries, &ring, &p) | panic_on_err("queue_init_params", false);
  }

  /** Destroy uio / io_uring object */
  ~uio() noexcept { io_uring_queue_exit(&ring); }

  // uio is not copyable. It can be moveable but humm...
  uio(const uio &) = delete;
  uio &operator=(const uio &) = delete;

 public:
  // TODO: there are a lot things to be resolved and done.
  //  a great design problem.... tough for me though
  void wait_for_completions() {
    io_uring_submit_and_wait(&ring, 1);
    io_uring_cqe *cqe;
    unsigned head;

    io_uring_for_each_cqe(&ring, head, cqe) {
      ++cqe_count;
      auto coro = static_cast<uio_token *>(io_uring_cqe_get_data(cqe));
      if (coro) coro->resolve(cqe->res);
    }
    io_uring_cq_advance(&ring, cqe_count);
    cqe_count = 0;
  }

  /**
   * Cancel a request
   * Attempt to cancel an already issued request.  addr must contain the user_data field of the request that
   * should be cancelled. The cancellation request will complete with one of the following results codes. If
   * found, the res field of the cqe will contain 0. If not found, res will contain -ENOENT.  If  found  and
   * attempted  cancelled,  the  res  field will contain -EALREADY. In this case, the request may or may not
   * terminate. In general, requests that are interruptible (like socket IO) will get cancelled, while  disk
   * IO requests cannot be cancelled if already started.  Available since 5.5.
   * @param user_data
   * @param flags
   * @param iflags
   * @return a task
   */
  uio_awaitable cancel(void *user_data, int flags = 0, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_cancel(sqe, user_data, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Read data into multiple buffers asynchronously
   * @see preadv2(2)
   * @see io_uring_enter(2) IORING_OP_READV
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable readv(int fd, const iovec *iovecs, unsigned nr_vecs, off_t offset, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_readv(sqe, fd, iovecs, nr_vecs, offset);
    return make_awaitable(sqe, iflags);
  }

  /** Write data into multiple buffers asynchronously
   * @see pwritev2(2)
   * @see io_uring_enter(2) IORING_OP_WRITEV
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable writev(int fd, const iovec *iovecs, unsigned nr_vecs, off_t offset, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_writev(sqe, fd, iovecs, nr_vecs, offset);
    return make_awaitable(sqe, iflags);
  }

  /** Read from a file descriptor at a given offset asynchronously
   * @see pread(2)
   * @see io_uring_enter(2) IORING_OP_READ
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable read(int fd, void *buf, unsigned nbytes, off_t offset, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_read(sqe, fd, buf, nbytes, offset);
    return make_awaitable(sqe, iflags);
  }

  /** Write to a file descriptor at a given offset asynchronously
   * @see pwrite(2)
   * @see io_uring_enter(2) IORING_OP_WRITE
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable write(int fd, const void *buf, unsigned nbytes, off_t offset, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_write(sqe, fd, buf, nbytes, offset);
    return make_awaitable(sqe, iflags);
  }

  /** Read data into a fixed buffer asynchronously
   * @see preadv2(2)
   * @see io_uring_enter(2) IORING_OP_READ_FIXED
   * @param buf_index the index of buffer registered with register_buffers
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable read_fixed(int fd, void *buf, unsigned nbytes, off_t offset, int buf_index,
                           uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_read_fixed(sqe, fd, buf, nbytes, offset, buf_index);
    return make_awaitable(sqe, iflags);
  }

  /** Write data into a fixed buffer asynchronously
   * @see pwritev2(2)
   * @see io_uring_enter(2) IORING_OP_WRITE_FIXED
   * @param buf_index the index of buffer registered with register_buffers
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable write_fixed(int fd, const void *buf, unsigned nbytes, off_t offset, int buf_index,
                            uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_write_fixed(sqe, fd, buf, nbytes, offset, buf_index);
    return make_awaitable(sqe, iflags);
  }

  /** Synchronize a file's in-core state with storage device asynchronously
   * @see fsync(2)
   * @see io_uring_enter(2) IORING_OP_FSYNC
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable fsync(int fd, unsigned fsync_flags, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_fsync(sqe, fd, fsync_flags);
    return make_awaitable(sqe, iflags);
  }

  /** Sync a file segment with disk asynchronously
   * @see sync_file_range(2)
   * @see io_uring_enter(2) IORING_OP_SYNC_FILE_RANGE
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned sync_range_flags,
                                uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_rw(IORING_OP_SYNC_FILE_RANGE, sqe, fd, nullptr, nbytes, offset);
    sqe->sync_range_flags = sync_range_flags;
    return make_awaitable(sqe, iflags);
  }

  /** Receive a message from a socket asynchronously
   * @see recvmsg(2)
   * @see io_uring_enter(2) IORING_OP_RECVMSG
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */

  uio_awaitable recvmsg(int sockfd, msghdr *msg, uint32_t flags, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_recvmsg(sqe, sockfd, msg, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Send a message on a socket asynchronously
   * @see sendmsg(2)
   * @see io_uring_enter(2) IORING_OP_SENDMSG
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable sendmsg(int sockfd, const msghdr *msg, uint32_t flags, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_sendmsg(sqe, sockfd, msg, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Receive a message from a socket asynchronously
   * @see recv(2)
   * @see io_uring_enter(2) IORING_OP_RECV
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable recv(int sockfd, void *buf, unsigned nbytes, uint32_t flags, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_recv(sqe, sockfd, buf, nbytes, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Send a message on a socket asynchronously
   * @see send(2)
   * @see io_uring_enter(2) IORING_OP_SEND
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable send(int sockfd, const void *buf, unsigned nbytes, uint32_t flags, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_send(sqe, sockfd, buf, nbytes, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Wait for an event on a file descriptor asynchronously
   * @see poll(2)
   * @see io_uring_enter(2)
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable poll(int fd, short poll_mask, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_poll_add(sqe, fd, poll_mask);
    return make_awaitable(sqe, iflags);
  }

  /** Enqueue a NOOP command, which eventually acts like pthread_yield when
   * awaiting, append: it would be waken up after some time,
   * but io_uring won't guarrantee an ordered execution of submission queue
   * to complete in the completion queue.
   * (even the assumption of fetch request in order might not meet expectation)
   * @see io_uring_enter(2) IORING_OP_NOP
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable yield(uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_nop(sqe);
    return make_awaitable(sqe, iflags);
  }

  /** Accept a connection on a socket asynchronously
   * @see accept4(2)
   * @see io_uring_enter(2) IORING_OP_ACCEPT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable accept(int fd, sockaddr *addr, socklen_t *addrlen, int flags = 0, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_accept(sqe, fd, addr, addrlen, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Initiate a connection on a socket asynchronously
   * @see connect(2)
   * @see io_uring_enter(2) IORING_OP_CONNECT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable connect(int fd, sockaddr *addr, socklen_t addrlen, int flags = 0, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_connect(sqe, fd, addr, addrlen);
    return make_awaitable(sqe, iflags);
  }

  /** Wait for specified duration asynchronously
   * @see io_uring_enter(2) IORING_OP_TIMEOUT
   * @param ts initial expiration, timespec
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable timeout(__kernel_timespec *ts, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_timeout(sqe, ts, 0, 0);
    return make_awaitable(sqe, iflags);
  }

  /** Open and possibly create a file asynchronously
   * @see openat(2)
   * @see io_uring_enter(2) IORING_OP_OPENAT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable openat(int dfd, const char *path, int flags, mode_t mode, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_openat(sqe, dfd, path, flags, mode);
    return make_awaitable(sqe, iflags);
  }

  /** Close a file descriptor asynchronously
   * @see close(2)
   * @see io_uring_enter(2) IORING_OP_CLOSE
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable close(int fd, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_close(sqe, fd);
    return make_awaitable(sqe, iflags);
  }

  /** Get file status asynchronously
   * @see statx(2)
   * @see io_uring_enter(2) IORING_OP_STATX
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable statx(int dfd, const char *path, int flags, unsigned mask, struct statx *statxbuf,
                      uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_statx(sqe, dfd, path, flags, mask, statxbuf);
    return make_awaitable(sqe, iflags);
  }

  /** Splice data to/from a pipe asynchronously
   * @see splice(2)
   * @see io_uring_enter(2) IORING_OP_SPLICE
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable splice(int fd_in, loff_t off_in, int fd_out, loff_t off_out, size_t nbytes, unsigned flags,
                       uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_splice(sqe, fd_in, off_in, fd_out, off_out, nbytes, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Duplicate pipe content asynchronously
   * @see tee(2)
   * @see io_uring_enter(2) IORING_OP_TEE
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable tee(int fd_in, int fd_out, size_t nbytes, unsigned flags, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_tee(sqe, fd_in, fd_out, nbytes, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Shut down part of a full-duplex connection asynchronously
   * @see shutdown(2)
   * @see io_uring_enter(2) IORING_OP_SHUTDOWN
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable shutdown(int fd, int how, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_shutdown(sqe, fd, how);
    return make_awaitable(sqe, iflags);
  }

  /** Change the name or location of a file asynchronously
   * @see renameat2(2)
   * @see io_uring_enter(2) IORING_OP_RENAMEAT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable renameat(int olddfd, const char *oldpath, int newdfd, const char *newpath, unsigned flags,
                         uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_renameat(sqe, olddfd, oldpath, newdfd, newpath, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Create a directory asynchronously
   * @see mkdirat(2)
   * @see io_uring_enter(2) IORING_OP_MKDIRAT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable mkdirat(int dirfd, const char *pathname, mode_t mode, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_mkdirat(sqe, dirfd, pathname, mode);
    return make_awaitable(sqe, iflags);
  }

  /** Make a new name for a file asynchronously
   * @see symlinkat(2)
   * @see io_uring_enter(2) IORING_OP_SYMLINKAT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable symlinkat(const char *target, int newdirfd, const char *linkpath, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_symlinkat(sqe, target, newdirfd, linkpath);
    return make_awaitable(sqe, iflags);
  }

  /** Make a new name for a file asynchronously
   * @see linkat(2)
   * @see io_uring_enter(2) IORING_OP_LINKAT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags,
                       uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_linkat(sqe, olddirfd, oldpath, newdirfd, newpath, flags);
    return make_awaitable(sqe, iflags);
  }

  /** Delete a name and possibly the file it refers to asynchronously
   * @see unlinkat(2)
   * @see io_uring_enter(2) IORING_OP_UNLINKAT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  uio_awaitable unlinkat(int dfd, const char *path, unsigned flags, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_unlinkat(sqe, dfd, path, flags);
    return make_awaitable(sqe, iflags);
  }

 private:
  uio_awaitable make_awaitable(io_uring_sqe *sqe, uint8_t iflags) noexcept {
    io_uring_sqe_set_flags(sqe, iflags);
    return uio_awaitable(sqe);
  }

 public:
  /** Get a sqe pointer that can never be NULL
   * @param ring pointer to inited io_uring struct
   * @return pointer to `io_uring_sqe` struct (not NULL)
   */
  [[nodiscard]] io_uring_sqe *io_uring_get_sqe_safe() noexcept {
    auto *sqe = io_uring_get_sqe(&ring);
    if (__builtin_expect(!!sqe, true)) {
      return sqe;
    } else {
      io_uring_cq_advance(&ring, cqe_count);
      cqe_count = 0;
      io_uring_submit(&ring);
      sqe = io_uring_get_sqe(&ring);
      if (__builtin_expect(!!sqe, true)) return sqe;
      panic("io_uring_get_sqe", ENOMEM);
    }
  }

 public:
  /** Register files for I/O
   * @param fds fds to register
   * @see io_uring_register(2) IORING_REGISTER_FILES
   */
  void register_files(std::initializer_list<int> fds) { register_files(fds.begin(), (unsigned int)fds.size()); }
  void register_files(const int *files, unsigned int nr_files) {
    io_uring_register_files(&ring, files, nr_files) | panic_on_err("io_uring_register_files", false);
  }

  /** Update registered files
   * @see io_uring_register(2) IORING_REGISTER_FILES_UPDATE
   */
  void register_files_update(unsigned off, int *files, unsigned nr_files) {
    io_uring_register_files_update(&ring, off, files, nr_files) | panic_on_err("io_uring_register_files", false);
  }

  /** Unregister all files
   * @see io_uring_register(2) IORING_UNREGISTER_FILES
   */
  int unregister_files() noexcept { return io_uring_unregister_files(&ring); }

 public:
  /** Register buffers for I/O
   * @param ioves array of iovec to register
   * @see io_uring_register(2) IORING_REGISTER_BUFFERS
   */
  template <unsigned int N>
  void register_buffers(iovec(&&ioves)[N]) {
    register_buffers(&ioves[0], N);
  }
  void register_buffers(const struct iovec *iovecs, unsigned nr_iovecs) {
    io_uring_register_buffers(&ring, iovecs, nr_iovecs) | panic_on_err("io_uring_register_buffers", false);
  }

  /** Unregister all buffers
   * @see io_uring_register(2) IORING_UNREGISTER_BUFFERS
   */
  int unregister_buffers() noexcept { return io_uring_unregister_buffers(&ring); }

 public:
  /** Return internal io_uring handle */
  [[nodiscard]] io_uring &get_ring_handle() noexcept { return ring; }

 private:
  io_uring ring;
  unsigned cqe_count = 0;
};

}  // namespace coring::detail

#endif  // CORING_UIO_HPP
