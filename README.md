libcoring is a C++ network library in Proactor pattern (@see: POSA2), and it is based on the new io_uring syscall of
linux and coroutine in C++ 20. It's under development now, the progress could be viewed in Github project page.

**For an overview of development document or to know how libcoring is arranged, please refer to [DEVLOG.md](./DEVLOG.md)
file.**

---

There WON'T BE any updating of DEVLOG.md file from now on, but all information of the developing are available in the
commit log, IN DETAILED.

The format of commit log is fixed:

```
type(scope): A BRIEF LINE \n
\n
 - detailed explaination 1
 - detailed explaination 2
 - TODO (maybe)
 - what's next (maybe)
```

---
This project learn from following library or repositories:

- proactor pattern [boost.asio](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio.html)
- use buffer selection of io_uring in practice [io_uring-echo-server](https://github.com/frevib/io_uring-echo-server)
- use io_uring in practice [Lord of the io_uring guide](https://github.com/shuveb/loti-examples)

This project learn and modified some codes from following library or repositories:

- use C++20 coroutine in practice: [cppcoro](https://github.com/lewissbaker/cppcoro)
- use io_uring in OOP way [liburing4cpp](https://github.com/CarterLi/liburing4cpp)