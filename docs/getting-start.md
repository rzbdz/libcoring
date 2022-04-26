### Setup the environment:

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

What described in this section is only available in branch `cancellation` since it involves a lot of changes on the
structures and design, check [here](https://github.com/rzbdz/libcoring/tree/cancellation), once it's stable, it would be
merged.

Cancellation with c++ coroutine and `io_uring` is difficult to implement, and there are concerns on the performance (
both memory usage and extra runtime overhead).

Anyway, the first usable cancellation is supported (limited) now, more tests would be done and more functionalities
would support cancellation soon.

The cancellation is working with `async_task`. Using lazy `task` as coroutine would not benefit from it. This
distinguishes the `fire-but-pickup-later` semantic and `await/async` semantic.

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

Current implementation doesn't support multiple io operation (in `async_task` way) sharing a same `io_cancel_source`, it
should be fixed.

more documents would be provided soon after the first usable version is merged into master branch.
