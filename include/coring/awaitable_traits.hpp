// Copyright 2017 Lewis Baker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#ifndef CORING_ASYNC_AWAITABLE_TRAITS
#define CORING_ASYNC_AWAITABLE_TRAITS

#include "coring/detail/async/get_awaiter.hpp"

#include <type_traits>

namespace coring {
template <typename T, typename = void>
struct awaitable_traits {};

template <typename T>
struct awaitable_traits<T, std::void_t<decltype(coring::detail::get_awaiter(std::declval<T>()))>> {
  using awaiter_t = decltype(coring::detail::get_awaiter(std::declval<T>()));

  using await_result_t = decltype(std::declval<awaiter_t>().await_resume());
};
}  // namespace coring

#endif
