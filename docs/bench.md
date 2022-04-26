So far, only few benchmarks is done... Mainly focus on the overhead of the framework using C++20 coroutine.

----

### Benchmark tools:

[webbench](https://github.com/EZLippi/WebBench),
[apache bench](https://httpd.apache.org/),
[rust_echo_bench](https://togithub.com/haraldh/rust_echo_bench)

---

### Baseline:

| item        | baseline            | target             |
|-------------|---------------------|--------------------|
| httpd       | webserver_liburing  | demo/http_server   |
| echo server | io_uring-echo-server | demo/echo_server   |

#### baseline:

- [webserver_liburing](https://github.com/shuveb/loti-examples/blob/master/webserver_liburing.c)
- [io_uring-echo-server](https://github.com/frevib/io_uring-echo-server)
  The source code is tweaked to prevent irrelevant errors such as closing fd using `io_uring` or direct syscall.

#### target:

- [demo/http_server](coring/demo/http_server.cpp)
- [demo/echo_server](coring/demo/echo_server.cpp)

### Basic results:

Since the number is not matched with the one in early io_uring benchmarks with other kernel version, here only post a
comparison instead of absolute numbers.

Without `SQPoll`, **100** concurrent clients:

| utility                      | QPS  | Throughput |
|------------------------------|------|------------|
| webbench        (120 bytes)  | 102% | 99%        |
| apache bench    (120 bytes)  | 101% | 102%       |
| rust-echo-bench (512 bytes)  | 96%  | N/A        |
| rust-echo-bench (2018 bytes) | 102% | N/A        |

More accurate data would be measured on native linux machine later.

---

### Configuration:

#### Machine Spec:

- Intel Core i7 6700h 2.60GHz 16GiB
- WSL2 with kernel 5.17.1 (user-compiled wsl kernel, M$ haven't released the latest one)

#### Conventions:

- In http bench, use a same 404 response packet on memory cache prevent irrelevant error. Run bench for 2 times each,
  gather the results and compute the average.
- In echo bench, the setting refer
  to [here](https://github.com/frevib/io_uring-echo-server/blob/master/benchmarks/benchmarks.md), though, couldn't
  reproduce the 100K QPS on both baseline and target programs (around 30K~), if this is relevant to the machine spec or
  kernel version it remain to be found out.

#### webbench:

```shell
webbench -c 100 -t 60 http://127.0.0.1:PORT/404/
```

#### ab:

```shell
ab -c 100 -n 100000 http://127.0.0.1:PORT/404/
```

#### rust-echo-bench:

check [here](https://github.com/frevib/io_uring-echo-server/blob/master/benchmarks/benchmarks.md).

---

### What about epoll:

Run two same configuration `http server`, both use `HTTP/1.0` and no `keepalive` option, the `http_server`
using `io_uring` has gain about **2X** QPS than epoll based http server.

Factors on different framework may affect the result, more testings need to be done. A practical plan is to separate the
`io_context` implementation and abstraction, then build the abstraction on top of both `epoll`, `io_uring` and
even `IORing` in windows 11 insider channel.

A not-controlled-comparison HTTP/1.0 server bench result using `apache bench`:

```shell
#### without Connection: keepalive option ####
# epoll based, use some regular epoll-based framework 
Requests per second:    5517.08 [#/sec] (mean)
# io_uring based, and use libcoring (with SQPOLL off)
Requests per second:    15703.96 [#/sec] (mean)
# io_uring based, and use libcoring (with SQPOLL on)
Requests per second:    19503.34 [#/sec] (mean)
```