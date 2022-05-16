// eof_error.hpp
// Created by PanJunzhong on 2022/5/2.
//
#include <stdexcept>

#ifndef CORING_EOF_ERROR_HPP
#define CORING_EOF_ERROR_HPP
namespace coring {
class eof_error : public std::runtime_error {
 public:
  eof_error() : std::runtime_error("encounter and EOF") {}
};
}  // namespace coring
#endif  // CORING_EOF_ERROR_HPP
