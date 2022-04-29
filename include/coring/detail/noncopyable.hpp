#ifndef CORING_UTILS_NONCOPYABLE_H
#define CORING_UTILS_NONCOPYABLE_H
#pragma once
namespace coring {
class noncopyable {
  // also non movable
 public:
  noncopyable(const noncopyable &) = delete;
  noncopyable &operator=(const noncopyable &) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};
}  // namespace coring

#endif  // CORING_UTILS_NONCOPYABLE_H