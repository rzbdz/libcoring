/// cancelled_operation.hpp
/// Created by panjunzhong@outlook.com on 2022/4/16.

#ifndef CORING_CANCELLED_ERROR_HPP
#define CORING_CANCELLED_ERROR_HPP
#include <stdexcept>

namespace coring {
class cancelled_error : std::runtime_error {
 public:
  cancelled_error() : std::runtime_error("cancelled operation") {}
};
}  // namespace coring
#endif  // CORING_CANCELLED_ERROR_HPP
