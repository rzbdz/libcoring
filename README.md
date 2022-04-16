### What 's this:

libcoring is a C++ network library in Proactor pattern (@see: POSA2), and it is based on the new io_uring syscall of
linux and coroutine in C++ 20. It's under development now, the progress could be viewed in Github project page. For an
overview of development document or to know how libcoring is arranged, please refer to [design document](docs/design.md)
file.

---

### Getting started:

There are a few demos available inside the [coring/demo](https://github.com/rzbdz/libcoring/tree/dev/coring/demo)
directory when the development is keep going , check them for basic usages of this \`library\`.

#### echo server

The \`hello world\` program of socket programing would be
an [echo server](https://github.com/rzbdz/libcoring/blob/dev/coring/demo/echo_server.cpp). Though using io_uring with
its features to get performance would involve a lot of works to read the manuals and discussions, an typically echo
server in libcoring with the [automatic buffer selection](https://lwn.net/Articles/815491/) feature on would look like
the codes below, the buffer family in libcoring do support reading or writing certain bytes from or to a socket with no
needs to deal with short count problem, demos would be provided with next version:

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

#### timeout

Another most used in socket programing would be timer, there are two types of timers in libcoring, one using
the `hrtimer` in kernel provided by io_uring, the other is in the user space. To specific a timeout with an async
operation, the codes would be like this, all async operations should support this:

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

#### Cancellation

I like this, it fulfills a real asynchronous programing style.

What described in this section is only available in branch `cancellation` since it involves a lot of changes on the
structures and design, check [here](https://github.com/rzbdz/libcoring/tree/cancellation), once it's stable, it would be
merged into dev, master later.

Cancellation with c++ coroutine and `io_uring` is difficult to implement, and there are concerns on the performance (
both memory usage and extra runtime overhead).

Anyway, the first usable cancellation is supported (limited) now, more tests would be done and more functionalities
would support cancellation soon.

What's noteworthy is that the cancellation is working with `async_task` while those use `task`(lazy) as coroutine would
not support cancellation. I think this distinguishes the `fire-but-pickup-later` semantic and `await/async` semantic
using lazy `task`.

A demo would be like this (another style of `connect_to with` timeout):

```cpp
task<> connect(io_context *ioc) {
  using namespace std::chrono_literals;
  try {
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
  catch_it;
}
```

Actually, the current implementation is not very useful when we want to detach multiple `async_task` in a single
coroutine. To support this case, we need to do some multiplexing and demultiplexing jobs. So we still need
general `cancellation_source` and  `cancellation_token`. The `io_cancel*` should derive from them.

Then we can write codes like this:

```cpp
// pseudo codes, won't compile
task<> do_something(io_context *ioc, cancellation_token token) {
  try {
    io_cancel_source src1, src2, src3; // current implementation limit one source per async io operation
    auto [endpoint1, endpoint2, endpoint3] = resolve_server_endpoints_frome_somewhere();
    auto promise1 = tcp::connect_to(endpoint1, src1.get_token()); // async_task, fire and forget, but do start !
    auto promise2 = tcp::connect_to(endpoint2, src2.get_token());
    auto promise3 = tcp::connect_to(endpoint3, src3.get_token());
    // we then generate some request data before (or interleave, we don't know) we connected to the host
    auto array = get_from_somewhere();
    auto result_container = get_from_somewhere2();
    for(auto i : array) for(auto j : i){
      if(token.cancel_requested()) break;
      result_container.push_back(compute_something_synchrounously(j));
    }
    if(token.cancel_requested()){
      if (!promise1.is_ready()) src1.try_cancel(*ioc); // async_task, fire and forget
      if (!promise2.is_ready()) src2.try_cancel(*ioc);
      if (!promise3.is_ready()) src3.try_cancel(*ioc);
      [[maybe_unused]] auto c1 = co_await promise1;
      [[maybe_unused]] auto c2 = co_await promise2; 
      [[maybe_unused]] auto c3 = co_await promise3; // here may throw if cancelled
      cleanning_up(); // if we use buffers for async operation, 
                      // we might want to release those buffers and regain the memories.
      co_return;
    }
    //.... more code here
    auto conn1 = co_await promise1;
    auto conn2 = co_await promise2; 
    auto conn3 = co_await promise3;
    socket_writer(conn1, result_container.to_buffer(), src1); // now the source could be reuse
    socket_writer(conn1, result_container.to_buffer(), src1); // async_task, fire and forget
    socket_writer(conn1, result_container.to_buffer(), src1);
  }
  catch_it;
}
task<>at_some_time_cancel_it(cancellation_source* src){
  auto res = co_await do_many_things(); // it could be a timeout or something
  if(res){
    co_await src.request_cancel();
  }
}
// then, at somewhere
void run() {
  io_context context;
  cancellation_source src;
  context.schedule(do_something(&context, src.get_token()));
  context.schedule(at_some_time_cancel_it(&src));
  context.run();
}

```

Then all functions in the library such as `write_certain`, `read_certain`, `connect_to` etc., later everything build on
top of these could support cancellation by simply passing a cancellation_token in.

But now, there are a lot of efforts to do. Then a question arises, why not just use C# and enjoy all functionality
provided by .NET, and they might have plans on supporting io_uring (together with IORing in Windows 11) as well ? Sad.

more documents would be provided soon after the first usable version is merged into master branch.

#### Setup the environment:

I should write some scripts for these procedures, now there is one available in `.github/workflows/all_compile.yml`
.

O/S requirements:

- linux with kernel 5.6 or later (>=5.15 are suggested).
- install [liburing](https://github.com/axboe/liburing) to the system.

Checklist:

- make sure g++-11 or compiler supports C++20 standard coroutine (no `std::experimental` needed) is installed.
- make sure cmake, make, build-essential, google test is installed globally.

Build and run the echo_server:

```shell
# root dir
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release 
cd coring/demo # or just use --build 
make echo_server
./echo_server
```

---

### Notes:

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

---

### Notice:

There won't be any updating of docs/design.md file from its last update, but all information of the developing are
available in the commit log, in detailed. If things change, this readme would be updated.

The format of commit log is sustained:

```
type(scope): A BRIEF LINE \n
\n
 - detailed explaination 1
 - detailed explaination 2
 - TODO (maybe)
 - what's next (maybe)
```