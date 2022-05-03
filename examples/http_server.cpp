// muduo_http_server.cpp
// Created by PanJunzhong on 2022/4/29.
//
#include "coring/io_context.hpp"
#include "coring/async_logger.hpp"
#include "coring/acceptor.hpp"
#include "coring/buffer_pool.hpp"
#include "coring/file.hpp"
#include "http1/http_context.hpp"
#include "http1/http_request.hpp"
#include "http1/http_response.hpp"
#include "coring/socket_writer.hpp"
#include "coring/socket_reader.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#define LOCAL_HOST "127.0.0.1"
#define ANY_IN "0.0.0.0"
#define DEFAULT_SERVER_PORT 8000
#define QUEUE_DEPTH 512

#define MAX_CONNECTIONS 4096
#define BUFFER_BLOCK_SIZE 2048
#define BUFFERS_COUNT MAX_CONNECTIONS

char buf[BUFFERS_COUNT * BUFFER_BLOCK_SIZE];

using namespace coring;
using namespace coring::http;
buffer_pool::id_t GID("MD");

std::stop_source *global_source;

void sigint_handler(int signo) { global_source->request_stop(); }
task<> send_header(tcp::connection *conn, HttpResponse *res) {
  buffer header_buf(512);
  res->appendToBuffer(&header_buf);
  co_await write_all(conn, &header_buf);
}
inline task<> send_bad_request(tcp::connection *conn, HttpResponse *res) {
  res->setStatusCode(HttpResponse::k400BadRequest);
  return send_header(conn, res);
}

task<> send_file(tcp::connection *conn, buffer_pool *pool, HttpResponse *res, const string &path) {
  std::unique_ptr<file_t> file;
  off_t total_size = 0;
  if (path == "public/bench") {
    res->setStatusCode(HttpResponse::k404NotFound);
    co_await send_header(conn, res);
    co_return;
  }
  try {
    file = co_await openat(path.data(), O_RDONLY, 0);
    struct ::statx *detail = co_await file->get_statx();
    if (S_ISDIR(detail->stx_mode)) {
      res->setStatusCode(HttpResponse::k404NotFound);
    } else {
      total_size = static_cast<off_t>(detail->stx_size);
      res->setStatusCode(HttpResponse::k200Ok);
    }
  } catch (coring::bad_file &e) {
    res->setStatusCode(HttpResponse::k404NotFound);
  }
  if (total_size > 0) {
    res->setContentTypeByPath(path);
    res->setContentLength(total_size);
  }
  co_await send_header(conn, res);
  if (total_size == 0) {
    co_return;
  }
  long sent_bytes = 0;
  while (sent_bytes < total_size) {
    auto bf = co_await pool->read(file->fd(), GID, sent_bytes);
    sent_bytes += static_cast<decltype(sent_bytes)>(bf->readable());
    co_await write_all(conn, bf.get());
  }
  LOG_TRACE("one file is responses: {} bytes", total_size);
}

task<> do_http(std::unique_ptr<tcp::connection> conn, buffer_pool *pool) {
  // LOG_TRACE("new client");
  http::HttpContext ctx;
  auto read_buffer = buffer(BUFFER_BLOCK_SIZE);
  bool close = false;
  try {
    for (; !close;) {
      using namespace std::chrono_literals;
      HttpResponse res;
      co_await read_some(conn.get(), &read_buffer);
      if (!ctx.parseRequest(&read_buffer)) {
        close = true;
        res.setCloseConnection(close);
        co_await send_bad_request(conn.get(), &res);
      } else if (ctx.gotAll()) {
        close = ctx.request().keepalive();
        res.setCloseConnection(close);
        co_await send_file(conn.get(), pool, &res, ctx.request().path());
      }
      ctx.reset();
    }
  } catch (std::exception &e) {
    LOG_INFO("something happened, connection end, msg: {}", e.what());
  }
}

task<> server(tcp::acceptor *actor, buffer_pool *pool, std::stop_token token) {
  co_await actor->better_enable();
  co_await pool->provide_group_contiguous(buf, BUFFER_BLOCK_SIZE, BUFFERS_COUNT, GID);
  try {
    while (!token.stop_requested()) {
      auto conn = co_await actor->accept();
      co_spawn(do_http(std::move(conn), pool));
    }
  } catch (std::exception &e) {
    LOG_INFO("something happened, server done, msg: {}", e.what());
    coro::get_io_context_ref().stop();
  }
}

struct config {
  log_level level;
  bool sq_poll;
};

int main(int argc, char *argv[]) {
  uint16_t port = DEFAULT_SERVER_PORT;
  if (argc > 1) {
    port = static_cast<uint16_t>(::atoi(argv[1]));
  }
  // setup signals
  auto sigint = signal_set::sigint_for_context();
  // setup single thread io_context
  // io_context context(QUEUE_DEPTH, IORING_SETUP_SQPOLL);
  io_context context(QUEUE_DEPTH);
  context.register_signals(sigint, sigint_handler);
  // chores
  std::stop_source src;
  global_source = &src;
  // enable async_logger
  async_logger logger{};
  logger.enable();
  coring::set_log_level(INFO);
  LOG_INFO("HTTP/1.0 Webserver is listening on port: {}", DEFAULT_SERVER_PORT);
  // init memory management
  buffer_pool pool{};
  // setup sockets
  tcp::acceptor acceptor(ANY_IN, port);
  // make sure we have collected all logs
  // prepare to run the server
  context.schedule(server(&acceptor, &pool, src.get_token()));
  // blocking until exit
  context.run();
  return 0;
}