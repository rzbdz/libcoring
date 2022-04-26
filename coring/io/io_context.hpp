
#pragma once
#ifndef CORING_IO_CONTEXT_HPP
#define CORING_IO_CONTEXT_HPP
#include <functional>
#include <system_error>
#include <chrono>
#include <thread>
#include <latch>
#include <sys/poll.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <sys/eventfd.h>
#include <deque>

#ifndef NDEBUG
#include <execinfo.h>
#endif

#include "coring/coring_config.hpp"

#include "coring/utils/noncopyable.hpp"
#include "coring/utils/debug.hpp"
#include "coring/utils/thread.hpp"
#include "coring/utils/io_utils.hpp"
#include "coring/utils/time_utils.hpp"
#include "coring/utils/execute.hpp"

#include "coring/async/task.hpp"
#include "coring/async/async_task.hpp"
#include "coring/async/when_all.hpp"
#include "coring/async/async_scope.hpp"
#include "coring/async/single_consumer_async_auto_reset_event.hpp"

#include "io_awaitable.hpp"
#include "timer.hpp"
#include "signals.hpp"

namespace coring {
namespace detail {
constexpr uint64_t EV_BIG_VALUE = 0x1fffffffffffffff;
constexpr uint64_t EV_SMALL_VALUE = 0x1;

// This class is adopted from project liburing4cpp (MIT license).
// It encapsulates most of liburing interfaces within a RAII class.
// I just add some methods.
// ---------------------------------------------------------------------------------------------------------------------
// Copyright © 2020 Carter Li
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the “Software”), to deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
class io_uring_context : noncopyable {
 public:
  /** Init uio / io_uring_context object
   * @see io_uring_setup(2)
   * @param entries Maximum sqe can be gotten without submitting
   * @param flags flags used to init io_uring_context
   * @param wq_fd existing io_uring_context ring_fd used by IORING_SETUP_ATTACH_WQ
   * @note uio is NOT thread safe, nor is liburing. When used in a
   *       multi-threaded program, it's highly recommended to create
   *       uio/io_uring_context instance per thread, and set
   * IORING_SETUP_ATTACH_WQ flag to make sure that kernel shares the only async
   * worker thread pool. See `IORING_SETUP_ATTACH_WQ` for detail.
   */
  io_uring_context(int entries = 64, uint32_t flags = 0, uint32_t wq_fd = 0) {
    io_uring_params p = {
        .flags = flags,
        .wq_fd = wq_fd,
    };
    io_uring_queue_init_params(entries, &ring, &p) | panic_on_err("queue_init_params", false);
  }
  /**
   * For multi-thread or SQPOLL usage
   * @param entries Maximum sqe can be gotten without submitting
   * @param p a params structure, you can only specify flags which can be used to set various flags and sq_thread_cpu
   * and sq_thread_idle fields
   * <p>p.flags could be one of these (most used):</p>
   * <p>IORING_SETUP_IOPOLL</p>
   * <p>IORING_SETUP_SQPOLL</p>
   * <p>IORING_SETUP_ATTACH_WQ, with this set, the wq_fd should be set to same fd to avoid multiple thread pool</p>
   * <p>other filed of the params are:</p>
   * <p>sq_thread_cpu: the cpuid, you can get one from gcc utility or something, I didn't find detailed document for
   * this field</p>
   * <p>sq_thread_idle: a milliseconds to stop sq polling thread.</p>
   */
  io_uring_context(int entries, io_uring_params p) {
    io_uring_queue_init_params(entries, &ring, &p) | panic_on_err("queue_init_params", false);
  }

  io_uring_context(int entries, io_uring_params *p) {
    io_uring_queue_init_params(entries, &ring, p) | panic_on_err("queue_init_params", false);
  }

  /** Destroy uio / io_uring_context object */
  // TODO: stop using inheritance
  virtual ~io_uring_context() noexcept { io_uring_queue_exit(&ring); }

 public:
  inline void wait_for_completions(int min_c = 1) { io_uring_submit_and_wait(&ring, min_c); }

  void handle_completions() {
    io_uring_cqe *cqe;
    unsigned head;

    io_uring_for_each_cqe(&ring, head, cqe) {
      ++cqe_count;
      auto coro = static_cast<io_token *>(io_uring_cqe_get_data(cqe));
      // support the timeout enter, if we have kernel support EXT_ARG
      // then this would be unnecessary
      if (coro != nullptr && coro != reinterpret_cast<void *>(LIBURING_UDATA_TIMEOUT))
        coro->resolve(cqe->res, cqe->flags);
    }
    io_uring_cq_advance(&ring, cqe_count);
    cqe_count = 0;
  }

  inline void wait_for_completions_then_handle() {
    wait_for_completions();
    handle_completions();
  }

 public:
  /**
   * Link a timeout to a async operation
   *
   * @param ts
   * @param flags timeout_flags may contain IORING_TIMEOUT_ABS for an absolute timeout value, or 0 for a relative
   * timeout
   * @param iflags IOSQE_* flags
   */
  void link_timeout(struct __kernel_timespec *ts, unsigned flags = 0, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_link_timeout(sqe, ts, flags);
    io_uring_sqe_set_flags(sqe, iflags);
  }
  /**
   * A helper method
   * Link a timeout to a async operation
   * timeout_flags may contain IORING_TIMEOUT_ABS for an absolute timeout value, or 0 for a relative timeout
   * @param ts
   * @param flags timeout_flags may contain IORING_TIMEOUT_ABS for an absolute timeout value, or 0 for a relative
   * timeout
   * @param iflags IOSQE_* flags
   */
  io_awaitable link_timeout(io_awaitable &&async_op, struct __kernel_timespec *ts, unsigned flags = 0,
                            uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_link_timeout(sqe, ts, flags);
    io_uring_sqe_set_flags(sqe, iflags);
    return async_op;
  }
  /**
   * Cancel a request
   * TODO: I won't deal with cancellation now...(0)
   * Cancellation is complicated, what boost.asio do is
   * just provide interface to cancel all async operations on specific socket (by closing it).
   * Maybe std::stop_source would be enough for most cases.
   * If need to close a socket/file, please make sure you use the IORING_OP_CLOSE with io_uring
   * for revoking all previous posts.
   * @see: https://patchwork.kernel.org/project/linux-fsdevel/patch/20191213183632.19441-9-axboe@kernel.dk/
   * ---------Linux manual:------
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
  io_awaitable cancel(io_cancel_token tk, int flags = 0, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_cancel(sqe, reinterpret_cast<__u64>(tk.get_cancel_key()), flags);
    return make_awaitable(sqe, iflags);
  }

  /** Read data into multiple buffers asynchronously
   * @see preadv2(2)
   * @see io_uring_enter(2) IORING_OP_READV
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  io_awaitable readv(int fd, const iovec *iovecs, unsigned nr_vecs, off_t offset, uint8_t iflags = 0) noexcept {
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
  io_awaitable writev(int fd, const iovec *iovecs, unsigned nr_vecs, off_t offset, uint8_t iflags = 0) noexcept {
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
  io_awaitable read(int fd, void *buf, unsigned nbytes, off_t offset, uint8_t iflags = 0) {
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
  io_awaitable write(int fd, const void *buf, unsigned nbytes, off_t offset, uint8_t iflags = 0) {
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
  io_awaitable read_fixed(int fd, void *buf, unsigned nbytes, off_t offset, int buf_index,
                          uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_read_fixed(sqe, fd, buf, nbytes, offset, buf_index);
    return make_awaitable(sqe, iflags);
  }

  /** Write data into a fixed buffer asynchronously
   * @see pwritev2(2)y
   * @see io_uring_enter(2) IORING_OP_WRITE_FIXED
   * @param buf_index the index of buffer registered with register_buffers
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  io_awaitable write_fixed(int fd, const void *buf, unsigned nbytes, off_t offset, int buf_index,
                           uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_write_fixed(sqe, fd, buf, nbytes, offset, buf_index);
    return make_awaitable(sqe, iflags);
  }

  /** Read from a file descriptor at a given offset using IOSQE_BUFFER_SELECT
   * @see man 2 pread, https://manpages.debian.org/unstable/liburing-dev/io_uring_enter.2.en.html#IOSQE_BUFFER_SELECT
   * @see io_uring_enter(2) IORING_OP_READ, IOSQE_BUFFER_SELECT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting, result is the bid or <=0 as error occurs.
   */
  io_awaitable_res_flag read_buffer_select(int fd, __u16 gid, unsigned nbytes, off_t offset = 0, uint8_t iflags = 0) {
    iflags |= IOSQE_BUFFER_SELECT;
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_read(sqe, fd, nullptr, nbytes, offset);
    sqe->buf_group = gid;
    return make_awaitable_res_flag(sqe, iflags);
  }

  /** Synchronize a file's in-core state with storage device asynchronously
   * @see fsync(2)
   * @see io_uring_enter(2) IORING_OP_FSYNC
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  io_awaitable fsync(int fd, unsigned fsync_flags, uint8_t iflags = 0) noexcept {
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
  io_awaitable sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned sync_range_flags,
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

  io_awaitable recvmsg(int sockfd, msghdr *msg, uint32_t flags, uint8_t iflags = 0) noexcept {
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
  io_awaitable sendmsg(int sockfd, const msghdr *msg, uint32_t flags, uint8_t iflags = 0) noexcept {
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
  io_awaitable recv(int sockfd, void *buf, unsigned nbytes, uint32_t flags, uint8_t iflags = 0) noexcept {
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
  io_awaitable send(int sockfd, const void *buf, unsigned nbytes, uint32_t flags, uint8_t iflags = 0) noexcept {
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
  io_awaitable poll(int fd, short poll_mask, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_poll_add(sqe, fd, poll_mask);
    return make_awaitable(sqe, iflags);
  }

  /** Enqueue a NOOP command, which eventually acts like pthread_yield when
   * awaiting, append: it would be waken up after some time,
   * but io_uring_context won't guarrantee an ordered execution of submission queue
   * to complete in the completion queue.
   * (even the assumption of fetch request in order might not meet expectation)
   * @see io_uring_enter(2) IORING_OP_NOP
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  io_awaitable yield(uint8_t iflags = 0) noexcept {
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
  io_awaitable accept(int fd, sockaddr *addr, socklen_t *addrlen, int flags = 0, uint8_t iflags = 0) noexcept {
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
  io_awaitable connect(int fd, const sockaddr *addr, socklen_t addrlen, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_connect(sqe, fd, addr, addrlen);
    return make_awaitable(sqe, iflags);
  }

  /** Wait for specified duration or wait until a absolute time asynchronously
   * A timeout will trigger a wakeup event on the completion ring for anyone waiting for events. A timeout condition is
   * met when either the specified timeout expires, or the specified number of events have completed. Either condition
   * will trigger the event. If set to 0, completed events are not counted, which effectively acts like a timer.
   * io_uring timeouts use the CLOCK_MONOTONIC clock source. The request will complete with -ETIME if the timeout got
   * completed through expiration of the timer, or 0 if the timeout got completed through requests completing on their
   * own. If the timeout was cancelled before it expired, the request will complete with -ECANCELED.
   * @see io_uring_enter(2) IORING_OP_TIMEOUT
   * @param ts initial expiration, timespec
   * @param timeout_flags may contain IORING_TIMEOUT_ABS for an absolute timeout value, or 0 for a relative
   * timeout
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  io_awaitable timeout(__kernel_timespec *ts, uint8_t flags = 0, uint8_t iflags = 0) noexcept {
    auto *sqe = io_uring_get_sqe_safe();
    //  off may contain a completion event count
    io_uring_prep_timeout(sqe, ts, 0, flags);
    return make_awaitable(sqe, iflags);
  }

  /**
   * WARNING: not fully tested...
   * TODO: manage the lifetime of timespec correctly...
   * Wait for specified duration asynchronously
   * A timeout will trigger a wakeup event on the completion ring for anyone waiting for events. A timeout condition is
   * met when either the specified timeout expires, or the specified number of events have completed. Either condition
   * will trigger the event. If set to 0, completed events are not counted, which effectively acts like a timer.
   * io_uring timeouts use the CLOCK_MONOTONIC clock source. The request will complete with -ETIME if the timeout got
   * completed through expiration of the timer, or 0 if the timeout got completed through requests completing on their
   * own. If the timeout was cancelled before it expired, the request will complete with -ECANCELED.
   * @see io_uring_enter(2) IORING_OP_TIMEOUT
   * @param dur a duration
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  template <typename Duration>
  requires(!std::is_same_v<__kernel_timespec *, std::remove_cvref<Duration>>)
      async_task<int> timeout(Duration &&dur, uint8_t flags = 0, uint8_t iflags = 0)
  noexcept {
    auto ks = make_timespec(std::forward<Duration>(dur));
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_timeout(sqe, &ks, 0, flags);
    co_return co_await make_awaitable(sqe, iflags);
  }

  /** Open and possibly create a file asynchronously
   * @see openat(2)
   * @see io_uring_enter(2) IORING_OP_OPENAT
   * @param iflags IOSQE_* flags
   * @return a task object for awaiting
   */
  io_awaitable openat(int dfd, const char *path, int flags, mode_t mode, uint8_t iflags = 0) noexcept {
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
  io_awaitable close(int fd, uint8_t iflags = 0) noexcept {
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
  io_awaitable statx(int dfd, const char *path, int flags, unsigned mask, struct statx *statxbuf,
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
  io_awaitable splice(int fd_in, loff_t off_in, int fd_out, loff_t off_out, size_t nbytes, unsigned flags,
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
  io_awaitable tee(int fd_in, int fd_out, size_t nbytes, unsigned flags, uint8_t iflags = 0) {
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
  io_awaitable shutdown(int fd, int how, uint8_t iflags = 0) {
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
  io_awaitable renameat(int olddfd, const char *oldpath, int newdfd, const char *newpath, unsigned flags,
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
  io_awaitable mkdirat(int dirfd, const char *pathname, mode_t mode, uint8_t iflags = 0) {
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
  io_awaitable symlinkat(const char *target, int newdirfd, const char *linkpath, uint8_t iflags = 0) {
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
  io_awaitable linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags,
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
  io_awaitable unlinkat(int dfd, const char *path, unsigned flags, uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_unlinkat(sqe, dfd, path, flags);
    return make_awaitable(sqe, iflags);
  }

 public:
  /**
   * If you are registering for the first time, just prepare len*how_many memory space,
   * then call len and how_many to be what it should be.
   * If you are calling this for reuse a pined buffer(that is, locked from used by kernel
   * when it's used for once), just set how_many to be one.
   * TODO: It 'd be better to provide fast and simplify interfaces for this usage.
   * https://lwn.net/Articles/815491/
   * A usage example would be available here:
   * https://github.com/frevib/io_uring-echo-server/blob/master/io_uring_echo_server.c
   * @see io_uring_enter(2) IORING_OP_PROVIDE_BUFFERS
   * @param addr the start address of current buffer
   * @param len_buffer len per buffer
   * @param how_many from addr together with first_bid start, how many to provide this time.
   * @param group_id group multiple buffers set by there size.
   * @param first_bid the first buffer id provided this time.
   * @param iflags just for IOSQE_* flags like LINK option
   * @return we need a task since this is a async operation, it's not like register_buffer.
   */
  io_awaitable provide_buffers(void *addr, int len_buffer, int how_many, __u16 group_id, __u16 first_bid,
                               uint8_t iflags = 0) {
    auto *sqe = io_uring_get_sqe_safe();
    io_uring_prep_provide_buffers(sqe, addr, len_buffer, how_many, (int)group_id, (int)first_bid);
    return make_awaitable(sqe, iflags);
  }

 private:
  io_awaitable make_awaitable(io_uring_sqe *sqe, uint8_t iflags) noexcept {
    io_uring_sqe_set_flags(sqe, iflags);
    return io_awaitable(sqe);
  }

  io_awaitable_flag make_awaitable_flag(io_uring_sqe *sqe, uint8_t iflags) noexcept {
    io_uring_sqe_set_flags(sqe, iflags);
    return io_awaitable_flag(sqe);
  }

  io_awaitable_res_flag make_awaitable_res_flag(io_uring_sqe *sqe, uint8_t iflags) noexcept {
    io_uring_sqe_set_flags(sqe, iflags);
    return io_awaitable_res_flag(sqe);
  }

 public:
  /** Get a sqe pointer that can never be NULL
   * @param ring pointer to inited io_uring_context struct
   * @return pointer to `io_uring_sqe` struct (not NULL)
   */
  [[nodiscard]] io_uring_sqe *io_uring_get_sqe_safe() noexcept {
    auto *sqe = io_uring_get_sqe(&ring);
    if (__builtin_expect(!!sqe, true)) {
      return sqe;
    } else {
      // If no sqe available, just try to submit it to kernel.
      // Here would block !!
      io_uring_cq_advance(&ring, cqe_count);
      cqe_count = 0;
      //  On success io_uring_submit(3) returns the number of submitted submission queue entries.
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
  /** Return internal io_uring_context handle */
  [[nodiscard]] ::io_uring &get_ring_handle() noexcept { return ring; }
  int ring_fd() const noexcept { return ring.ring_fd; }

 protected:
  ::io_uring ring{};

 private:
  unsigned cqe_count = 0;
};

}  // namespace detail

///
/// Manual for multi-thread or SQPOLL usage
/// @param entries Maximum sqe can be gotten without submitting
/// @param p a params structure, you can only specify flags which can be used to set various flags and sq_thread_cpu
///  and sq_thread_idle fields
/// < p>p.flags could be one of these (most used):</p>
/// < p>IORING_SETUP_IOPOLL</p>
/// < p>IORING_SETUP_SQPOLL</p>
/// < p>IORING_SETUP_ATTACH_WQ, with this set, the wq_fd should be set to same fd to avoid multiple thread pool</p>
/// < p>other filed of the params are:</p>
/// < p>sq_thread_cpu: the cpuid, you can get one from gcc utility or something, I didn't find detailed document for
///  this field</p>
/// < p>sq_thread_idle: a milliseconds to stop sq polling thread.</p>
///
class io_context : public coring::detail::io_uring_context {
 public:
  typedef io_context *executor_t;

 private:
  typedef coring::task<> my_todo_t;
  static constexpr uint64_t EV_STOP_MSG = detail::EV_BIG_VALUE;
  static constexpr uint64_t EV_WAKEUP_MSG = detail::EV_SMALL_VALUE;

  void create_eventfd() {
    // NONBLOCK for direct write, no influence on io_uring_read
    // internal_event_fd_ = ::eventfd(0, EFD_NONBLOCK);
    internal_event_fd_ = ::eventfd(0, 0);
    if (internal_event_fd_ == -1) {
      // TODO: error handling
      // it's another story.
      // either use the exception or simply print some logs and abort.
      // https://isocpp.org/wiki/faq/exceptions#ctors-can-throw
      throw std::runtime_error("o/s fails to allocate more fd");
    }
  }

  void create_signalfd(signal_set &s) {
    internal_signal_fd_ = ::signalfd(-1, s.get_set(), 0);
    if (internal_signal_fd_ == -1) {
      // TODO: error handling
      // it's another story.
      // either use the exception or simply print some logs and abort.
      // https://isocpp.org/wiki/faq/exceptions#ctors-can-throw
      throw std::runtime_error("o/s fails to allocate more fd");
    }
  }

 public:
  io_context(int entries = 64, uint32_t flags = 0, uint32_t wq_fd = 0)
      : detail::io_uring_context{entries, flags, wq_fd} {
    create_eventfd();
  }

  /// stupid name, only for dev channel...
  /// \return a io_context isntance.
  static inline io_context dup_from_big_brother(io_context *bro, int entries = 64) {
    auto fl = bro->ring.flags;
    if (fl & IORING_SETUP_SQPOLL) {
      // You can only get real entries from ring->sz or the para when you init one ,we just pass in one...
      // RVO should work here.
      return io_context{entries, fl | IORING_SETUP_ATTACH_WQ, static_cast<uint32_t>(bro->ring_fd())};
    } else {
      return io_context{entries, fl};
    }
  }

  io_context(int entries, io_uring_params p) : detail::io_uring_context{entries, p} { create_eventfd(); }

  io_context(int entries, io_uring_params *p) : detail::io_uring_context{entries, p} { create_eventfd(); }

  void register_signals(signal_set &s, __sighandler_t func = nullptr) {
    create_signalfd(s);
    signal_func_ = func;
  }

 private:
  coring::async_run init_signalfd(__sighandler_t func) {
    struct signalfd_siginfo siginfo {};
    do {
      co_await read(internal_signal_fd_, &siginfo, sizeof(siginfo), 0, 0);
      if (func != nullptr) {
        func(siginfo.ssi_signo);
      }
      if (siginfo.ssi_signo == SIGINT) {
        stopped_ = true;
      }
    } while (!stopped_);
  }
  /// this coroutine should start (and be suspended) at io_context start
  coring::async_run init_eventfd() {
    uint64_t msg;
    while (!stopped_) {
      // Don't use LOG_DEBUG_RAW since it's not thread safe (only use when testing)
      // LOG_DEBUG_RAW("co_await the eventfd!, must be inside of the loop");
      co_await read(internal_event_fd_, &msg, 8, 0, 0);
      // TODO: I don't know if this is a good solution..
      // since strict-ordering memory model are available on current x86 CPUs.
      // But if volatile restrict program to read from memory, it would be costly.
      if (msg >= EV_STOP_MSG) {
        stopped_ = true;
      }
    }
  }

  /// TODO: I just find a other approaches to implement user level timer...
  /// In the current version, I just post requests of timeout everytime I wakeup, that is
  /// stupid since there is a api- timeout who rely on a hrtimer in the kernel.
  /// @see https://lwn.net/Articles/800308/
  /// But the order of completions rises by prep_timeout is not guaranteed and thus I think
  /// we do need a user timer...(or just use linked request). Also, when the amount is large,
  /// I don't think the hrtimer(rbtree) would be better (we can do testing though).
  /// The other approach is just use wait_cqe_with_timeout/io_uring_enter,
  /// just like what we did with epoll before.
  /// @see: https://github.com/axboe/liburing/issues/4
  /// \return
  async_run init_timeout_callback() {
    while (!stopped_) {
      while (timer_.has_more_timeouts()) {
        auto tmp = timer_.get_next_expiration();
        co_await timeout(&tmp);
        timer_.handle_events();
      }
      co_await timer_event_;
    }
  }

  void notify(uint64_t msg) {
    if (::write(internal_event_fd_, &msg, sizeof(msg)) == -1) {
      // TODO: do something
      // it's ok since eventfd isn't been read.
      // when it's going to stop, it's fine
      // to stop it synchronously.
      ;
    }
    // TODO: do something (logging)
  }

  void do_todo_list() {
    std::vector<my_todo_t> local_copy{};
    local_copy.swap(todo_list_);
    for (auto &t : local_copy) {
      // user should not pass any long running task in here.
      // that is to say, a task with co_await inside instead of
      // some blocking task...
      execute(std::move(t));
    }
  }

  // TODO: I won't deal with cancellation now...(2)
  // Cancellation is complicated, what boost.asio do is
  // just provide interface to cancel all async operations on specific socket (by closing it).
  // Maybe std::stop_source would be enough for most cases.
  // If need to close a socket/file, please make sure you use the IORING_OP_CLOSE with io_uring
  // for revoking all previous posts.
  // @see: https://patchwork.kernel.org/project/linux-fsdevel/patch/20191213183632.19441-9-axboe@kernel.dk/
  void do_cancel_list() { throw std::invalid_argument("not available in this channel"); }

 public:
  inline executor_t as_executor() { return this; }

  bool inline on_this_thread() { return reinterpret_cast<decltype(this)>(coring::thread::get_key_data(0)) == this; }

  /// Immediate issue a one-way-task to run the awaitable.
  /// Make sure you are in the current thread (or, inside of a coroutine running on the io_context).
  /// check io_context::spawn(...).
  template <typename AWAITABLE>
  void execute(AWAITABLE &&awaitable) {
    // now it was call inside of the wait_for_completion,
    // so it would be submitted soon (at next round of run)
    my_scope_.spawn(std::forward<AWAITABLE>(awaitable));
  }

  /// Just don't use this, check io_context::spawn(...).
  void schedule(my_todo_t &&awaitable) { todo_list_.emplace_back(std::move(awaitable)); }

  void schedule(std::vector<my_todo_t> &task_list) {
    if (todo_list_.empty()) {
      std::swap(todo_list_, task_list);
    } else {
      std::move(task_list.begin(), task_list.end(), std::back_inserter(todo_list_));
      task_list.clear();
    }
  }
  /// set timeout
  /// \tparam AWAITABLE
  /// \param awaitable
  template <typename AWAITABLE, typename Duration>
  void run_after(AWAITABLE &&awaitable, Duration &&duration) {}

 public:
  // on the same thread...
  void register_timeout(std::coroutine_handle<> cont, std::chrono::microseconds exp) {
    // TODO: if no this wrapper, the async_run would be exposed to user.
    // Actually this two argument is all POD...no move required.
    timer_.add_event(cont, exp);
    timer_event_.set();
  }

  void wakeup() { notify(EV_WAKEUP_MSG); }

  inline void submit() { wakeup(); }

  /// Spawn a task on current event loop
  ///
  /// Since liburing is not thread-safe(especially with io_uring_get_sqe), we
  /// CANNOT spawn anything related to a io_context sqe manipulation outside of
  /// the context thread, usually new task would be spawned when the get_completion
  /// goes to.
  ///
  /// Updates: after some investigation, I think we don't need any multithreading here,
  /// just forbids it. If we need threading, do our own or checkout another branch to try it out.
  ///
  /// \param awaitable a task, must be void return such as task<>
  void inline spawn(my_todo_t &&awaitable) { execute(std::move(awaitable)); }

  ~io_context() noexcept override {
    // have to free resources
    ::close(internal_event_fd_);
    if (internal_signal_fd_ != -1) {
      ::close(internal_signal_fd_);
    }
    // have to co_await async_scope
    // FIXME: dangerous here.. we lost the scope, memory leaking
    // we can maintain a list of task/coroutine, then destroy then all, but it's not simple to impl.
    // another better solution would be using a timeout for all operation, but it damage the performance for
    // creating lots of linked timeout to the hrtimer in kernel.
    // a better one should be done using the cancellation, we can just keep a list of user_data (maybe using a
    // std::unordered_set) and cancel all of them.
    [a = this]() -> async_run { co_await a->my_scope_.join(); }();
  }

 private:
  /// the best practice might be using a eventfd in io_uring_context
  /// or manage your timing event using a rb-tree timing wheel etc. to
  /// use IORING_OP_TIMEOUT like timerfd in epoll?
  /// more info: @see:https://kernel.dk/io_uring_context-whatsnew.pdf
  void do_run() {
    // bind thread.
    stopped_ = false;
    init_eventfd();
    // do scheduled tasks
    if (internal_signal_fd_ != -1) {
      init_signalfd(signal_func_);
    }
    do_todo_list();
    init_timeout_callback();
    while (!stopped_) {
      // the coroutine would be resumed inside io_token.resolve() method
      // blocking syscall. Call io_uring_submit_and_wait.
      wait_for_completions_then_handle();
      do_todo_list();
    }
    // TODO: handle stop event, deal with async_scope (issue cancellations then call join) exiting
  }

 public:
  void run() {
    coring::thread::set_key_data(this, 0);
    do_run();
    coring::thread::set_key_data(nullptr, 0);
  }

  void run(std::latch &cd) {
    coring::thread::set_key_data(this, 0);
    cd.count_down();
    do_run();
    coring::thread::set_key_data(nullptr, 0);
  }

  void stop() {
    if (reinterpret_cast<coring::io_context *>(coring::thread::get_key_data(0)) == this) {
      stopped_ = true;
    } else {
      // lazy stop
      // notify this context, then fall into the run()-while loop-break,
      notify(EV_STOP_MSG);
    }
  }

 private:
  // io_uring_context engine inherited from uio
  // a event fd for many uses
  int internal_event_fd_{-1};
  int internal_signal_fd_{-1};
  std::mutex mutex_;
  // no volatile for all changes are made in the same thread
  // not using stop_token for better performance.
  // TODO: should we use a atomic and stop using eventfd msg to demux ?
  bool stopped_{true};
  // for co_spawn
  std::vector<my_todo_t> todo_list_{};
  // TODO: I won't deal with cancellation now...(1)
  // for cancelling
  std::vector<io_cancel_token> to_cancel_list_{};
  coring::async_scope my_scope_{};
  coring::timer timer_{};
  coring::single_consumer_async_auto_reset_event timer_event_;
  __sighandler_t signal_func_{nullptr};
};
}  // namespace coring

namespace coring {
struct coro {
  static auto get_io_context() {
    auto ptr = reinterpret_cast<coring::io_context *>(coring::thread::get_key_data(0));
    // NO exception thrown, just make it support nullptr
    return ptr;
  }
  static io_context &get_io_context_ref() {
    auto ptr = reinterpret_cast<coring::io_context *>(coring::thread::get_key_data(0));
    if (ptr == nullptr) {
      throw std::runtime_error{"no io_context bind"};
    }
    return *ptr;
  }
  /// Just make sure you are in a coroutine before calling this,
  /// it's not the same spawn as the one in boost::asio.
  static void spawn(task<> &&t) { get_io_context_ref().spawn(std::move(t)); }
};
}  // namespace coring

#endif  // CORING_IO_CONTEXT_HPP
