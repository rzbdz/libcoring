libcoring is a C++ network library in Proactor pattern (@see: POSA2), and it is based on the new io_uring syscall of
linux and coroutine in C++ 20. It's under development now, the progress could be viewed in Github project page.

For an overview of development document or to know how libcoring is arranged, please refer to [DEVLOG.md](./DEVLOG.md)
file.

---
---
This project learn and modified some codes from below projects:

- use C++20 coroutine in practice: [cppcoro](https://github.com/lewissbaker/cppcoro)
- use io_uring in OOP way [liburing4cpp](https://github.com/CarterLi/liburing4cpp)
- use io_uring in practice [io_uring-echo-server](https://github.com/frevib/io_uring-echo-server)
- proactor pattern [boost.asio](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio.html)