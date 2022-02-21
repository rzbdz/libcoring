libcoring is a C++ network library in Proactor pattern (@see: POSA2), and it is based on the new io_uring syscall of
linux and coroutine in C++ 20. It's under development now, nothing is completed so far.

Nothing of this library is completed so far, by the time when the first usable version is completed, this file would be
updated.

---

### Design Pattern

proactor pattern in libcoring tcp server (might not be the final version):
![proactor pattern image](https://github.com/rzbdz/libcoring/blob/dev/.res/proactor_model.png )

The basic pattern of the libcoring is **Paroactor** (combined with **Asynchronous-Completion-Token** and
**Acceptor-Connector**
patterns as POSA2 stated), which is tightly cooperated with the asynchronous I/O operation interfaces provided by the
O/S (**io_uring** in linux kernel 5.6 and after).

The proactor entity is named `io_context` (just like the one in boost::asio). In current design, one io_context is
linked to one io_uring submission queue, and also one thread (to avoid complicated synchronizing).

The high level abstraction won't need explicit calls to `io_context` object to do threading (but the acceptor main
loop (proactor) thread context is still required as Acceptor-Connector pattern works), everything about the connection
threading would be delegated to class called `tcp_server` or `tcp_client`.

When it comes to **UDP**, things become trivial for the `recv()` and `send()` always make the full packet works,
protocols such as **QUIC(HTTP/3)** over UDP won't be part of the libcoring 's job (but demo might be provided).

### Why Proactor

**Asynchronous I/O** is fast for less overhead brought by context switches and data copying between kernel and user
space (
especially after the meltdown & spectre).

**Reactor** and `epoll` based network library is popular and **Proactor**, the asynchronous model is adopted by fewer
libraries in the past for linux doesn't have a good application interface while windows application benefits a lot from
the **I/O Completion Port** interface in usability and performance together with **await/async** coroutine (Task)
concept in C#.

However, with the new `io_uring` interfaces introduced in linux kernel 5 and the new compiler level implementation of
coroutine in C++20, things begin will change.

### Boost::asio

Boost::asio do support `io_uring` and coroutine for C++20 now (C++17 TS demo). This project would learn from asio as
well. There is no other reasons not using boost::asio.

For now, this **libcoring** library would only be a (simple toy level when it's under development) new (using C++20 and
linux kernel 5.6 api) and lightweight linux-only project, who need not deal with old version compatibility problem and
platform differences.

---

### Logging module

The logging module of libcoring use [fmtlib](https://github.com/fmtlib/fmt)
as front end. But both formatting job and persistence happens in background thread. By default, the
`async_logger` would start a new thread(`std::jthread`) running **polling** procedure. It might delegate the thread to
thread pool to manage. Different polling policies would be introduced. Normally, the trade-off is between latency and
high throughput, when both wanted, only binary logging would be qualified, together with some facilities to preprocess
and decode the logs (which means formatting offline).

To reduce the overhead of lock contention when different thread submitting simultaneously, it maintains a
**single-producer-single-consumer** lock-free queue per thread. The SPSC queue is simpler modeled after the C
implementation in Intel DPDK [libring](https://github.com/DPDK/dpdk/tree/main/lib/ring) in C++ class.

The timestamp of every log message uses `std::chrono::system_clock::now()` directly, which lead to a high portion of
latency when other low latency logging system would use CPU ticks (like `rdtsc` instruction) and estimate time (
nanoseconds) on the fly, for simplicity I didn't use one, but the timestamp acquiring would occupy 50% ~ 90% of the
total submitting latency. Thus, this implementation would be not that useful when the logging argument is not so complex
for the bottleneck is elsewhere.

In current implementation, polling of background thread simply **batch popping** messages from lock-free queue which
choose in **round-robin** policy, and there are no contention between producer and consumer if the queue is not full (
which should be the usual case of network applications except sometime busy loop logging like connection retrying
occurs). The `async_logger` itself is a state machine and can be stopped or signaled by other thread, every 3 seconds a
persistence would be forced to occur (flush logs to file). If there is no log entry available (a.k.a. empty queue for
all threads)
, `async_logger` thread would go to sleep and be wakened up either a timout or a signal occurs. And the `log_file` do
support
**rolling** automatically, more configuration options should be added.

Since the formatting would require two memory buffers, one for messages from front-end, one for formatted logs. This
implementation doesn't use double buffer, instead it uses a simple on stack buffer, for simple application it should be
enough. Alternatives like using mmap or io_uring (async io would use techs like DMA) would be taken into consideration.

The results only be better(equal in else cases) when the front-end is not busy logging and the log string is not very
simple (for example:
multiple digits combined with string formatting), the latency of front end is stable (**2X faster** than
**spdlog** while logging complex message).


