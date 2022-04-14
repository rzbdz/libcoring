### What 's this:

libcoring is a C++ network library in Proactor pattern (@see: POSA2), and it is based on the new io_uring syscall of
linux and coroutine in C++ 20. It's under development now, the progress could be viewed in Github project page. For an
overview of development document or to know how libcoring is arranged, please refer to [design document](docs/design.md)
file.

---

### Getting started:

There are a few demos available when the development is keep going inside
the [coring/demo](https://github.com/rzbdz/libcoring/tree/dev/coring/demo) directory, check them for knowing the basic
usages of this \`library\`.

The \`hello world\` program of socket programing would be
an [echo server](https://github.com/rzbdz/libcoring/blob/dev/coring/demo/echo_server.cpp). Though using io_uring with
its features to get performance (both time & space) might be a lot of works to read the manuals and discussions on it,
an echo server in libcoring with the [automatic buffer selection](https://lwn.net/Articles/815491/) feature on would
typically look like this:

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

Another most used in socket programing would be timer, there are two types of timers in libcoring, one using
the `hrtimer` in kernel provided by io_uring, the other is in the user space. To specific a timeout with an async
operation, the codes would be like this, all async operations should support this:

```cpp
// codes are trimmed, check the source `coring/demo/connect_with_timeout.cpp` for details
task<> connect() {
  using namespace std::chrono_literals;
  auto endpoint = net::endpoint::from_resolve("www.google.com", 80);
  std::cout << endpoint.address_str() << std::endl;
  auto conn = co_await tcp::connect_to(endpoint, 3s); // it would throw an exception if timeout
  // ... do sth with conn
}

void run() {
  io_context context;
  context.schedule(connect(&context));
  context.run();
}
```

more documents would be updated soon after the first usable version is merged into branch master.


---

### Notes:

This project learn from following library or repositories:

- proactor pattern [boost.asio](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio.html)
- use buffer selection of io_uring in practice [io_uring-echo-server](https://github.com/frevib/io_uring-echo-server)
- use io_uring in practice [Lord of the io_uring guide](https://github.com/shuveb/loti-examples)

This project learn and modified some codes from following library or repositories:

- use C++20 coroutine in practice: [cppcoro](https://github.com/lewissbaker/cppcoro)
- use io_uring in OOP way [liburing4cpp](https://github.com/CarterLi/liburing4cpp)

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