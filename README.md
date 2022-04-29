### What 's this:

libcoring is a C++ network library in Proactor pattern (@see: POSA2), and it is based on the new io_uring syscall of
linux and coroutine in C++ 20. It's under development now, the progress could be viewed in Github project page.

There are some [documents](docs/) available:

- For an overview of development document or to know how libcoring is arranged, please refer
  to [design document](docs/design.md)
  file.
- To build and run this library with demos, check [getting start](docs/getting-start.md) file. Installation and library
  import/link configuration isn't available yet for it's still developing.

---

### Getting Start

Just take a look at the directory [demo](coring/demo/).

#### echo server:

```cpp
// codes are trimmed, check the source `coring/demo/echo_server.cpp` for details
task<> echo_loop(tcp::connection conn){
  while(true){
    auto& selected = co_await buffer_pool.try_read_block(conn, GID, MAX_MESSAGE_LEN);
    selected_buffer_resource return_it_when_exit(selected);
    co_await socket_writer(conn, selected).write_all_to_file();
  }
}
task<> event_loop() {
  co_await buffer_pool.provide_group_contiguous(buf, MAX_MESSAGE_LEN, BUFFERS_COUNT, GID);
  while (true) {
    auto conn = co_await acceptor.accept();
    coro::spawn(echo_loop(conn));
  }
}
void run(){
  io_context context;
  context.schedule(event_loop());
  context.run();
}
```

#### timeout:

```cpp
// codes are trimmed, check the source `coring/demo/connect_with_timeout.cpp` for details
task<> connect() {
  using namespace std::chrono_literals;
  auto endpoint = net::endpoint::from_resolve("www.google.com", 80);
  std::cout << endpoint.address_str() << std::endl;
  auto conn = co_await tcp::connect_to(endpoint, 3s); // timer in kernel, it would throw an exception if timeout
  // ... do sth with conn
  // ....................
  co_await timeout(20s); // timer in user space , see coring/test/io_context_test.cpp
                         // with coroutine, this is natual
  // ... do sth else
  // ....................
}

void run() {
  io_context context;
  context.schedule(connect(&context));
  context.run();
}
```

#### cancellation:

Notice: since `IORING_ASYNC_CANCEL_ANY` in io_uring will soon be released, the development on cancellation part of
libcoring stalls now. The cancellation branch would be marked decrapted and be deleted soon. By the time the new kernel
is available, the development would go on with another branch. Codes below may be not supported or supported in newer
versions.

```cpp
task<> connect(io_context *ioc) {
  using namespace std::chrono_literals;
  io_cancel_source src;
  auto ep = net::endpoint::from_resolve("www.google.com", 80);
  // non-blocking, fire-and-forget, async_task
  auto promise = tcp::connect_to(ep, src.get_token()); 
  // I can just do many other things here, say do some O(n^2) computing...
  // ......
  // use this for simplicity, user space timer is low cost-effective here...
  co_await ioc->timeout(3s);
  if (!promise.is_ready()) {
    auto res = co_await src.cancel_and_wait_for_result(*ioc);
    // res is 0 if successfully cancelled
  }
  [[maybe_unused]] auto c = co_await promise; 
  // here may throw if cancelled, I didn't decide if we should count ECLEAN as an exception
}
```

For more details, check  [getting start](docs/getting-start.md) file.

---

### Performance

Now the comparison is focus on the overhead of the utilities and coroutine abstraction in libcoring, when it comes to
io_uring versus epoll/... is another topics. Currently, only httpd and echo-server is benched.
Check [benchmark](docs/bench.md) page for details.

Without SQPoll, compared to raw C liburing interface  (100%):

| utility                      | QPS  | Throughput |
|------------------------------|------|------------|
| webbench        (120 bytes)  | 102% | 99%        |
| apache bench    (120 bytes)  | 101% | 102%       |
| rust-echo-bench (512 bytes)  | 96%  | N/A        |
| rust-echo-bench (2018 bytes) | 102% | N/A        |

---

### Focusing on now

- Cancellation for io_uring.

---

### Notices and Concerns:

This project learn from following library or repositories:

- proactor pattern [boost.asio](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio.html)
- use buffer selection of io_uring in practice [io_uring-echo-server](https://github.com/frevib/io_uring-echo-server)
- use io_uring in practice [Lord of the io_uring guide](https://github.com/shuveb/loti-examples)
- executor model (might be accepted in C++23) [libunifex](https://github.com/facebookexperimental/libunifex)

This project learn and modified some codes from following library or repositories:

- use C++20 coroutine in practice: [cppcoro](https://github.com/lewissbaker/cppcoro)
- use io_uring in OOP way [liburing4cpp](https://github.com/CarterLi/liburing4cpp)

The type of coroutine `task<>` is lazy in libcoring, just like the `awaitable<>` in boost::asio, arguments on
lazy `task<>` or non-lazy `task<>` both make sense, the difference is only in the `initial_suspend` function, I would
try to find which is better design, maybe lazy task cooperate with executor would be enough:

- [P1056R0: task](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1056r0.html)
- [P0443R12: executor](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0443r12.html)
- [Fire and forget](https://togithub.com/lewissbaker/cppcoro/issues/145)
- [Cancellation Problem]( https://togithub.com/CarterLi/liburing4cpp/issues/27)
- [my non-lazy task impl]( https://togithub.com/rzbdz/libcoring/commit/bd5ef1e5b2532a800673f9bc115aa131f7aec5c1)
- [another C++2a coroutine library](https://togithub.com/Quuxplusone/coro)

The cancellation for io_uring and combined with stackless coroutine would be a little difficult to image. There are
mature solution in C# await and async facilities, but IOCP differs to io_uring on both cancellation and multithreading
restrictions. Although a simple cancellation token could be used for coroutine, but the supports for io_uring would be
another subject. Another things is that the cancellation part in io_uring is still keep going, recently the CANCEL_ANY
is patched, cancellation is now supporting cancel more than one request a time (use a fd). But it requires a further
upgrade of latest kernel, I would try once it's available.

- [cppcoro::Cancellation](https://github.com/lewissbaker/cppcoro#Cancellation)
- [Cancellation in C#](https://docs.microsoft.com/en-us/dotnet/api/system.threading.cancellationtoken?view=net-6.0)
- [Use Cancellation in C#](https://stackoverflow.com/questions/15067865/how-to-use-the-cancellationtoken-property)
- [one io_uring patch](https://lore.kernel.org/all/20220418164402.75259-4-axboe@kernel.dk/)
- [another io_uring patch](https://lore.kernel.org/all/20220418164402.75259-6-axboe@kernel.dk/)

---