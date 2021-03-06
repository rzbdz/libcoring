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
#ifndef CORING_ASYNC_ON_SCOPE_EXIT
#define CORING_ASYNC_ON_SCOPE_EXIT

#include <type_traits>
#include <exception>

namespace coring {
template <typename FUNC>
class scoped_lambda {
 public:
  scoped_lambda(FUNC &&func) : m_func(std::forward<FUNC>(func)), m_cancelled(false) {}

  scoped_lambda(const scoped_lambda &other) = delete;

  scoped_lambda(scoped_lambda &&other) : m_func(std::forward<FUNC>(other.m_func)), m_cancelled(other.m_cancelled) {
    other.cancel();
  }

  ~scoped_lambda() {
    if (!m_cancelled) {
      m_func();
    }
  }

  void cancel() { m_cancelled = true; }

  void call_now() {
    m_cancelled = true;
    m_func();
  }

 private:
  FUNC m_func;
  bool m_cancelled;
};

/// A scoped lambda that executes the lambda when the object destructs
/// but only if exiting due to an exception (CALL_ON_FAILURE = true) or
/// only if not exiting due to an exception (CALL_ON_FAILURE = false).
template <typename FUNC, bool CALL_ON_FAILURE>
class conditional_scoped_lambda {
 public:
  conditional_scoped_lambda(FUNC &&func)
      : m_func(std::forward<FUNC>(func)), m_uncaughtExceptionCount(std::uncaught_exceptions()), m_cancelled(false) {}

  conditional_scoped_lambda(const conditional_scoped_lambda &other) = delete;

  conditional_scoped_lambda(conditional_scoped_lambda &&other) noexcept(std::is_nothrow_move_constructible<FUNC>::value)
      : m_func(std::forward<FUNC>(other.m_func)),
        m_uncaughtExceptionCount(other.m_uncaughtExceptionCount),
        m_cancelled(other.m_cancelled) {
    other.cancel();
  }

  ~conditional_scoped_lambda() noexcept(CALL_ON_FAILURE || noexcept(std::declval<FUNC>()())) {
    if (!m_cancelled && (is_unwinding_due_to_exception() == CALL_ON_FAILURE)) {
      m_func();
    }
  }

  void cancel() noexcept { m_cancelled = true; }

 private:
  bool is_unwinding_due_to_exception() const noexcept { return std::uncaught_exceptions() > m_uncaughtExceptionCount; }

  FUNC m_func;
  int m_uncaughtExceptionCount;
  bool m_cancelled;
};

/// Returns an object that calls the provided function when it goes out
/// of scope either normally or due to an uncaught exception unwinding
/// the stack.
///
/// \param func
/// The function to call when the scope exits.
/// The function must be noexcept.
template <typename FUNC>
auto on_scope_exit(FUNC &&func) {
  return scoped_lambda<FUNC>{std::forward<FUNC>(func)};
}

/// Returns an object that calls the provided function when it goes out
/// of scope due to an uncaught exception unwinding the stack.
///
/// \param func
/// The function to be called if unwinding due to an exception.
/// The function must be noexcept.
template <typename FUNC>
auto on_scope_failure(FUNC &&func) {
  return conditional_scoped_lambda<FUNC, true>{std::forward<FUNC>(func)};
}

/// Returns an object that calls the provided function when it goes out
/// of scope via normal execution (ie. not unwinding due to an exception).
///
/// \param func
/// The function to call if the scope exits normally.
/// The function does not necessarily need to be noexcept.
template <typename FUNC>
auto on_scope_success(FUNC &&func) {
  return conditional_scoped_lambda<FUNC, false>{std::forward<FUNC>(func)};
}
}  // namespace coring

#endif
