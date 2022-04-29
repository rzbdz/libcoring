/// http_helper.hpp
/// Created by panjunzhong@outlook.com on 2022/4/25.

#ifndef CORING_HTTP_HELPER_HPP
#define CORING_HTTP_HELPER_HPP

#include <cctype>
#include <cstring>
#include <cstdio>

#include "coring/socket_writer.hpp"
#include "coring/socket_reader.hpp"

enum http_version { v10, v11, v20 };
enum http_method { GET, POST, PUT, MALFORM };

struct http_req_line {
  http_method method{MALFORM};
  std::string_view url{};
  http_version ver{v10};
};

http_req_line parse_http_method(char *request_line);

void prepare_http_headers(coring::flex_buffer &buf, const char *req_path, off_t content_length);

void prepare_filename(char *buf, std::string_view &url);

const char http_400_content[] =
    "HTTP/1.0 400 Bad Request\r\nContent-type: text/html\r\n\r\n"
    "<html><head><title>400 Bad Request</title></head>"
    "<body><h1>Bad Request</h1></body></html>";

const char http_404_content[] =
    "HTTP/1.0 404 Not Found\r\nContent-type: text/html\r\n\r\n"
    "<html><head><title>Coring: Not Found</title>/head>"
    "<body><h1>404 Not Found</h1><p>Please check the URL</p></body></html>";

#endif  // CORING_HTTP_HELPER_HPP
