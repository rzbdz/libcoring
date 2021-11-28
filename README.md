libcoring is a C++ network library in Proactor pattern (@see: POSA2), and it is based on the new io_uring syscall of
linux and coroutine in C++ 20. It's under development now, nothing is completed so far.

Nothing of this library is completed so far, by the time when the first usable version is completed, this file would be
updated.

## Design Pattern

![proactor pattern in libcoring tcp server](https://github.com/rzbdz/libcoring/blob/dev/.res/proactor_model.png)

The basic pattern of the libcoring is Paroactor (combined with Asynchronous-Completion-Token and Acceptor-Connector
patterns as POSA2 stated), which is tightly cooperated with the asynchronous I/O operation interfaces provided by the
O/S (io_uring in linux kernel 5.6 and after).

## Why Proactor

Asynchronous I/O is fast for less overhead brought by context switch and data copying between kernel and user space (
especially after the meltdown & spectre).

Reactor and epoll based network library is popular and Proactor, the asynchronous model is adopted by fewer libraries in
the past for linux doesn't have a good application interface while windows application benefits a lot from the I/O
Completion Port interface in usability and performance together with await/async concept in C#.

However, with the new io_uring interfaces introduced in linux kernel 5 and the new compiler level implementation of
coroutine in C++20, things begin will change.

## Boost::asio

Boost::asio do support io_uring and coroutine for C++ now, but I don't think the coroutine implementation is natural to
understand. This library would be a new (using C++20 and linux kernel 5.6 api) and lightweight linux-only project, who
need not deal with old version compatibility problem and platform differences.