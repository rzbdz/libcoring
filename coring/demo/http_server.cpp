/// http_server.cpp
/// This is a very trivial http server,
/// it even doesn't support keepalive.
#include "coring/io/io_context.hpp"
#include "coring/logging/async_logger.hpp"
#include "coring/net/acceptor.hpp"
#include "coring/net/file.hpp"
#include "http_helper.hpp"

#define LOCAL_HOST "127.0.0.1"
#define ANY_IN "0.0.0.0"
#define DEFAULT_SERVER_PORT 8000
#define QUEUE_DEPTH 256

#define MAX_CONNECTIONS 4096
#define BUFFER_BLOCK_SIZE 2048
#define BUFFERS_COUNT MAX_CONNECTIONS

using namespace coring;

char buf[BUFFERS_COUNT * BUFFER_BLOCK_SIZE];

buffer_pool::id_t GID("AB");

std::stop_source *global_source;

void sigint_handler(int signo) { global_source->request_stop(); }

task<> do_http(tcp::connection conn, buffer_pool &pool) {
  try {
    // normal http GET method would be typically 1KB ~ 8KB, depends on the headers, url length and conent...
    // We have to make sure that there is no bytes left behind in the receive buffer when we shutdown/close
    // the socket, or there would be an RST packet sent to the client.
    std::vector<char> parsing_buffer(1024);
    auto reader = socket_reader(conn, 1024);
    auto view = co_await reader.read_till_crlf();
    // parsing directly on buffer
    auto req = parse_http_method(const_cast<char *>(view.data()));
    if (req.method == GET) {
      prepare_filename(parsing_buffer.data(), req.url);
      LOG_INFO("GET for file final filename: {}, try async open file", parsing_buffer.data());
      auto file = co_await file::openat<detailed_file_t>(parsing_buffer.data(), O_RDONLY, 0);
      if (file.invalid()) {
        LOG_INFO("Invalid filename, send 404");
        co_await coro::get_io_context_ref().send(conn, http_404_content, sizeof(http_404_content), 0);
      } else {
        try {
          co_await file.fill_statx();
          auto detail = file.status_view();
          auto total_size = static_cast<off_t>(detail->stx_size);
          // write the HTTP request header to socket
          {
            auto writer = socket_writer(conn, 32);  // content-length: xxxx, 30 would be enough
            prepare_http_headers(writer.raw_buffer(), parsing_buffer.data(), total_size);
            co_await writer.write_all_to_file();
          }
          // write file as content to socket
          long sent_bytes = 0;
          while (sent_bytes < total_size) {
            auto &bf = co_await pool.try_read_block(file, GID, sent_bytes);  // splice is not available
            selected_buffer_resource on_exit{bf};
            sent_bytes += static_cast<decltype(sent_bytes)>(bf.readable());
            co_await socket_writer(conn, bf).write_all_to_file();
          }
        } catch (std::exception &e) {
          LOG_INFO("something happened, connection exit, msg: {}", e.what());
        }
        file.close();
      }
    } else {
      co_await coro::get_io_context_ref().send(conn, http_400_content, sizeof(http_400_content), 0);
    }
  } catch (std::exception &e) {
    LOG_INFO("something happened, connection exit, msg: {}", e.what());
  }
  co_await conn.close();
  LOG_INFO("one connection exit");
}

task<> server(tcp::acceptor &actor, buffer_pool &pool, std::stop_token token) {
  co_await pool.provide_group_contiguous(buf, BUFFER_BLOCK_SIZE, BUFFERS_COUNT, GID);
  try {
    while (!token.stop_requested()) {
      auto conn = co_await actor.accept();
      coro::spawn(do_http(conn, pool));
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

int main() {
  // setup signals
  auto sigint = signal_set::sigint_for_context();
  // setup single thread io_context
  io_context context(QUEUE_DEPTH);
  context.register_signals(sigint, sigint_handler);
  // chores
  std::stop_source src;
  global_source = &src;
  // enable async_logger
  async_logger logger{};
  logger.enable();
  coring::set_log_level(coring::LOG_LEVEL_CNT);
  LOG_INFO("HTTP/1.0 Webserver is listening on port: {}", DEFAULT_SERVER_PORT);
  // init memory management
  buffer_pool pool{};
  // setup sockets
  tcp::acceptor acceptor(ANY_IN, DEFAULT_SERVER_PORT);
  acceptor.enable();
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