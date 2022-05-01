// muduo_http_server.cpp
// Created by PanJunzhong on 2022/4/29.
//
#include "coring/io_context.hpp"
#include "coring/async_logger.hpp"
#include "coring/acceptor.hpp"
#include "coring/file.hpp"
#include "http_helper.hpp"
#include "http1/http_context.hpp"
#include "http1/http_request.hpp"
#include "http1/http_response.hpp"

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
task<> send_header(tcp::connection conn, HttpResponse *res) {
  auto sw = socket_writer(conn, 512);
  res->appendToBuffer(&sw.raw_buffer());
  co_await sw.write_all_to_file();
}
task<> send_file(tcp::connection conn, buffer_pool &pool, HttpResponse *res, const string &path) {
  auto file = co_await file::openat<detailed_file_t>(path.data(), O_RDONLY, 0);
  off_t total_size = 0;
  HttpResponse::HttpStatusCode status;
  string status_msg;
  try {
    if (file.invalid()) {
      status = HttpResponse::k404NotFound;
      status_msg = "404 Not Found";
    } else {
      co_await file.fill_statx();
      struct ::statx *detail = file.status_view();
      if (S_ISDIR(detail->stx_mode)) {
        status = HttpResponse::k404NotFound;
        status_msg = "404 Not Found";
        co_await file.close();
      } else {
        total_size = static_cast<off_t>(detail->stx_size);
        status = HttpResponse::k200Ok;
        status_msg = "200 OK";
      }
    }
    res->setStatusCode(status);
    res->setStatusMessage(status_msg);
    res->setContentLength(total_size);
    if (total_size) {
      res->setContentTypeByPath(path);
      co_await send_header(conn, res);
      // write file as content to socket
      long sent_bytes = 0;
      try {
        while (sent_bytes < total_size) {
          auto &bf = co_await pool.try_read_block(file, GID, sent_bytes);  // splice is not available
          selected_buffer_resource on_exit{bf};
          sent_bytes += static_cast<decltype(sent_bytes)>(bf.readable());
          co_await socket_writer(conn, bf).write_all_to_file();
        }
      } catch (std::exception &e) {
        LOG_INFO("something happened, send file exit, msg: {}", e.what());
      }
      co_await file.close();
      // LOG_TRACE("one file is responses: {} bytes", total_size);
    } else {
      co_await send_header(conn, res);
    }
  } catch (std::exception &e) {
    LOG_INFO("something happened, send file exit, msg: {}", e.what());
  }
}

task<> do_http(tcp::connection conn, buffer_pool &pool) {
  http::HttpContext ctx;
  auto rd = socket_reader(conn, BUFFER_BLOCK_SIZE);
  // LOG_TRACE("new client");
  try {
    bool close = false;
    while (!close) {  // for keepalive option
      // TODO: use timing wheel to do keepalive detecting.
      using namespace std::chrono_literals;
      co_await rd.read_some();
      // LOG_TRACE("read some back, inside 3s still");
      if (!ctx.parseRequest(&rd.as_buffer(), timestamp{})) {
        // bad request
        close = true;
        HttpResponse res(close);
        res.setStatusCode(HttpResponse::k400BadRequest);
        res.setStatusMessage("400 Bad Request");
        co_await send_header(conn, &res);
        ctx.reset();
      } else if (ctx.gotAll()) {
        // one request end
        auto &req = ctx.request();
        const string &connection = req.getHeader("Connection");
        // FIXME: should use tolowercase...
        close = connection == "close" ||
                (req.getVersion() == HttpRequest::kHttp10 && connection != "keep-alive" && connection != "Keep-Alive");
        HttpResponse res(close);
        if (ctx.request().path() == "public/") {
          HttpResponse::HttpStatusCode status;
          string status_msg;
          status = HttpResponse::k404NotFound;
          status_msg = "404 Not Found";
          res.setStatusCode(status);
          res.setStatusMessage(status_msg);
          co_await send_header(conn, &res);
        } else {
          co_await send_file(conn, pool, &res, ctx.request().path());
        }
        ctx.reset();
      }
    }
  } catch (std::exception &e) {
    LOG_INFO("something happened, connection end, msg: {}", e.what());
  }
  co_await conn.close();  // FIXME: RAII for coroutine...should we use shared_ptr?
}

task<> server(tcp::acceptor &actor, buffer_pool &pool, std::stop_token token) {
  co_await actor.better_enable();
  co_await pool.provide_group_contiguous(buf, BUFFER_BLOCK_SIZE, BUFFERS_COUNT, GID);
  try {
    while (!token.stop_requested()) {
      auto conn = co_await actor.accept();
      co_spawn(do_http(conn, pool));
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
  __u16 port = DEFAULT_SERVER_PORT;
  if (argc > 1) {
    port = static_cast<uint16_t>(::atoi(argv[1]));
  }
  // setup signals
  auto sigint = signal_set::sigint_for_context();
  // setup single thread io_context
  io_context context(QUEUE_DEPTH, IORING_SETUP_SQPOLL);
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
  try {
    // prepare to run the server
    context.schedule(server(acceptor, pool, src.get_token()));
    // blocking until exit
    context.run();
  } catch (std::exception &e) {
    LOG_FATAL("io_uring down, msg: {}", e.what());
  }
  return 0;
}