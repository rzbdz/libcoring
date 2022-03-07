libcoring is a C++ network library in Proactor pattern (@see: POSA2), and it is based on the new io_uring syscall of
linux and coroutine in C++ 20. It's under development now, nothing is completed so far.

Nothing of this library is completed so far, by the time when the first usable version is completed, this file would be
updated.

For the development document, please refer to [DEVLOG.md](./DEVLOG.md) file

---

## Main Module:
- utils: some utilities for other module or library user (e.g. threading, buffer).
- logging (depend on utils): an async logging module, it performs formatting and disk writing
  for less latency (less than 1us in wsl2, regular benchmarks would be done).
- async: supports for C++20 coroutine.
- io (depend on async): io_uring and proactor (event loop) supports based on coroutine.
- net (depend on io): supports for socket and tcp connection.
- test: test for all modules using google test. 