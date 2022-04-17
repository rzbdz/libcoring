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

Actually, the current implementation is not very useful when we want to detach multiple `async_task` in a single
coroutine. In #10, it describes a general use case where there is only one io operation is pending on io_uring instance.
But in reality, we do want to submit multiple io operation request in a single coroutine.

To support this, we need to do some multiplexing and demultiplexing jobs. So we still need general `cancellation_source`
and  `cancellation_token`. The `io_cancel*` should derive from them.

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