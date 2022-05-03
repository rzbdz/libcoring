
#ifndef CORING_FILE_HPP
#define CORING_FILE_HPP
#include "file_descriptor.hpp"
namespace coring {

class bad_file : public std::system_error {
 public:
  bad_file(std::error_code ec) : std::system_error{ec} {}
};

struct null_status {};
struct full_status {
  // It 's too big, so argument cannot be passed by register...
  struct ::statx *statx_buf_{nullptr};
  /// Return a ptr to struct statx:
  /// <p>struct statx {</p>
  /// <p>     __u32 stx_mask;        /* Mask of bits indicating</p>
  /// <p>                               filled fields */</p>
  /// <p>     __u32 stx_blksize;     /* Block size for filesystem I/O */</p>
  /// <p>     __u64 stx_attributes;  /* Extra file attribute indicators */</p>
  /// <p>     __u32 stx_nlink;       /* Number of hard links */</p>
  /// <p>     __u32 stx_uid;         /* User ID of owner */</p>
  /// <p>     __u32 stx_gid;         /* Group ID of owner */</p>
  /// <p>     __u16 stx_mode;        /* File type and mode */</p>
  /// <p>     __u64 stx_ino;         /* Inode number */</p>
  /// <p>     __u64 stx_size;        /* Total size in bytes */</p>
  /// <p>     __u64 stx_blocks;      /* Number of 512B blocks allocated */</p>
  /// <p>     __u64 stx_attributes_mask;</p>
  /// <p>                            /* Mask to show what's supported in stx_attributes */</p>
  /// <p>     /* The following fields are file timestamps */</p>
  /// <p>     struct statx_timestamp stx_atime;  /* Last access */</p>
  /// <p>     struct statx_timestamp stx_btime;  /* Creation */</p>
  /// <p>     struct statx_timestamp stx_ctime;  /* Last status change */</p>
  /// <p>     struct statx_timestamp stx_mtime;  /* Last modification */</p>
  /// <p>     /* If this file represents a device, then the next two fields contain the ID of the device */</p>
  /// <p>     __u32 stx_rdev_major;  /* Major ID */</p>
  /// <p>     __u32 stx_rdev_minor;  /* Minor ID */</p>
  /// <p>     /* The next two fields contain the ID of the device containing the filesystem where the file resides
  /// */</p>
  /// <p>     __u32 stx_dev_major;   /* Major ID */</p> <p>     __u32 stx_dev_minor;   /* Minor ID */</p> <p> };
  [[nodiscard]] const struct ::statx *view() const { return statx_buf_; }
  [[nodiscard]] struct ::statx *view() { return statx_buf_; }

  full_status(const full_status &src) {
    if (src.statx_buf_ != nullptr) {
      statx_buf_ = new struct statx;
      *statx_buf_ = *(src.statx_buf_);
    }
  }
  /// we have to do this so as to support tast return
  full_status(full_status &&src) noexcept {
    statx_buf_ = src.statx_buf_;
    src.statx_buf_ = nullptr;
  }
  full_status() = default;
  ~full_status() {
    // deleting nullptr is fine...
    delete statx_buf_;
  }
  void init() { statx_buf_ = new struct statx; }
};
template <typename>
class file_base;
typedef file_base<null_status> empty_file_t;
typedef file_base<full_status> file_t;
template <typename StatusType>
class file_base : public file_descriptor {
 public:
  explicit file_base(int ffd) : file_descriptor{ffd} {}
  file_base(const file_base &rhs) : file_descriptor{rhs.fd_}, status_{rhs.status_} {}
  file_base(file_base &&rhs) noexcept : file_descriptor{rhs.fd_}, status_{std::move(rhs.status_)} {}
  int fd() const { return fd_; }
  /// Fill the statx lazily...
  /// @see: statx(2)
  /// \param mask
  /// The  mask  argument  to statx() is used to tell the kernel which fields the caller is interested in.
  /// mask is an ORed
  /// <p>      combination of the following constants:</p>
  /// <p>          STATX_TYPE          Want stx_mode & S_IFMT</p>
  /// <p>          STATX_MODE          Want stx_mode & ~S_IFMT</p>
  /// <p>          STATX_NLINK         Want stx_nlink</p>
  /// <p>          STATX_UID           Want stx_uid</p>
  /// <p>          STATX_GID           Want stx_gid</p>
  /// <p>          STATX_ATIME         Want stx_atime</p>
  /// <p>          STATX_MTIME         Want stx_mtime</p>
  /// <p>          STATX_CTIME         Want stx_ctime</p>
  /// <p>          STATX_INO           Want stx_ino</p>
  /// <p>          STATX_SIZE          Want stx_size</p>
  /// <p>          STATX_BLOCKS        Want stx_blocks</p>
  /// <p>          STATX_BASIC_STATS   [All of the above]</p>
  /// <p>          STATX_BTIME         Want stx_btime</p>
  /// <p>          STATX_ALL           [All currently available fields]</p>
  async_task<struct ::statx *> get_statx(uint mask = STATX_BASIC_STATS) {
    static_assert(std::is_same_v<StatusType, full_status>);
    if (status_.view() == nullptr) {
      status_.init();
      auto ret = co_await coro::get_io_context_ref().statx(fd_, "\0", AT_EMPTY_PATH, mask, status_.view());
      if (ret < 0) {
        throw coring::bad_file(std::error_code{-ret, std::system_category()});
      }
    }
    co_return status_.view();
  }

  /// I think this would be a class provides low-level interfaces,
  /// so no exception are thrown here...
  /// People may have different argument on whether throw or not in EOF case.
  /// We don't provide timeout interface since the regular file is usually cannot be
  /// cancelled. If user want one anyway, they can implement their own. (see the one in `socket` class)
  /// \param dst
  /// \param nbytes expected count, short read may occurs, one most common case is a file bigger than 2GB
  ///        other case when kernel buffer is insufficient would occurs.
  /// \return bytes really read from file
  inline detail::io_awaitable read(char *dst, size_t nbytes, off_t off = 0) {
    return coro::get_io_context_ref().read(fd_, (void *)dst, (unsigned)nbytes, off);
  }

  /// I think this would be a class provides low-level interfaces,
  /// so no exception are thrown here...
  /// People may have different argument on whether throw or not in EOF case.
  /// We don't provide timeout interface since the regular file is usually cannot be
  /// cancelled. If user want one anyway, they can implement their own. (see the one in `socket` class)
  /// \param dst
  /// \param nbytes expected count, short read may occurs, one most common case is a file bigger than 2GB
  ///        other case when kernel buffer is insufficient would occurs.
  /// \return bytes really written to file
  inline detail::io_awaitable write(const char *src, size_t nbytes, off_t off = 0) {
    return coro::get_io_context_ref().write(fd_, (void *)src, (unsigned)nbytes, off);
  }

 private:
  StatusType status_;
};

/// openat can fully replace the open...
/// first, if you need a diectly, just use absolute path and set  O_PATH as flag
/// to open a directory fd, then pass it to another openat call with relative path,
/// you will get what you want.
/// \param UseExecption I don't know, I may want different configurations in diff use cases.
/// \param dfd use AT_FDCWD to indicate the current working directory
/// \param path relative path or absolute one
/// \param flags O_RDONLY, O_WRONLY, O_RDWR ... just read the manual
/// \param mode  The mode argument specifies the file mode bits be applied when a new file is created.
///              e.g. S_IRWXU = 00700, user read write execute, S_IWGRP group write
/// \return
template <class FileStorageType = file_t, bool UseExecption = true>
inline async_task<std::unique_ptr<FileStorageType>> openat(const char *path, int flags, mode_t mode = 0,
                                                           int dirfd = AT_FDCWD) {
  auto ffd = co_await coro::get_io_context_ref().openat(dirfd, path, flags, mode);
  if constexpr (UseExecption) {
    if (ffd < 0) {
      throw coring::bad_file(std::error_code{-ffd, std::system_category()});
    }
  }
  // http://eel.is/c++draft/class.copy.elision#3
  // TODO: useless move, we will see
  co_return std::move(std::make_unique<FileStorageType>(ffd));
}
}  // namespace coring

#endif  // CORING_FILE_HPP
