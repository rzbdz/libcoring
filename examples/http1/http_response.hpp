// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// Part of the code is modified by me
#ifndef CORING_HTTP_RESPONSE_HPP
#define CORING_HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <iterator>
#include "coring/detail/copyable.hpp"
#include "coring/timestamp.hpp"
#include "coring/buffer.hpp"

using coring::timestamp;
using std::string;
namespace coring::http {
class HttpResponse : public coring::copyable {
 public:
  enum HttpStatusCode {
    kUnknown,
    k200Ok = 200,
    k301MovedPermanently = 301,
    k400BadRequest = 400,
    k404NotFound = 404,
  };

  explicit HttpResponse(bool close) : statusCode_(kUnknown), closeConnection_(close) {}
  explicit HttpResponse() : statusCode_(kUnknown), closeConnection_(false) {}

  void setStatusCode(HttpStatusCode code) {
    statusCode_ = code;
    switch (statusCode_) {
      case kUnknown:
        // setStatusMessage("");
        break;
      case k200Ok:
        setStatusMessage("OK");
        break;
      case k301MovedPermanently:
        setStatusMessage("Move Permanently");
        break;
      case k400BadRequest:
        setStatusMessage("Bad Request");
        break;
      case k404NotFound:
        setStatusMessage("Not Found");
        break;
    }
  }

 private:
  void setStatusMessage(const string &message) { statusMessage_ = message; }

 public:
  void setCloseConnection(bool on) { closeConnection_ = on; }

  bool closeConnection() const { return closeConnection_; }

  void setContentType(const string &contentType) { addHeader("Content-Type", contentType); }

  void setContentLength(size_t len) { addHeader("Content-Length", std::to_string(len)); }

#define GEN_CONTENT_TYPE(valname, type) \
  auto valname = type;                  \
  static_cast<void>(0)
#define TRY_SET_CONTENT_TYPE(suffix, type_str) \
  if (strcmp(#suffix, file_ext) == 0)          \
    setContentType(type_str);                  \
  else

  const char *get_filename_ext(const char *filename) {
    const char *dot = ::strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
  }
  void strtolower(char *str) {
    for (; *str; ++str) *str = (char)::tolower(*str);
  }

  void setContentTypeByPath(const string &path) {
    char small_case_path[1024];
    strcpy(small_case_path, path.data());
    strtolower(small_case_path);
    // turn the extension into lower case before checking.
    const char *file_ext = get_filename_ext(small_case_path);
    // setup content-types
    GEN_CONTENT_TYPE(JPEG, "image/jpeg");
    GEN_CONTENT_TYPE(PNG, "image/png");
    GEN_CONTENT_TYPE(GIF, "image/gif");
    GEN_CONTENT_TYPE(HTML, "text/html");
    GEN_CONTENT_TYPE(JS, "application/javascript");
    GEN_CONTENT_TYPE(CSS, "text/css");
    GEN_CONTENT_TYPE(TXT, "text/plain");
    GEN_CONTENT_TYPE(MP4, "video/mpeg4");
    // bind suffix
    TRY_SET_CONTENT_TYPE(jpeg, JPEG)
    TRY_SET_CONTENT_TYPE(png, PNG)
    TRY_SET_CONTENT_TYPE(jpg, JPEG)
    TRY_SET_CONTENT_TYPE(gif, GIF)
    TRY_SET_CONTENT_TYPE(htm, HTML)
    TRY_SET_CONTENT_TYPE(html, HTML)
    TRY_SET_CONTENT_TYPE(js, JS)
    TRY_SET_CONTENT_TYPE(css, CSS)
    TRY_SET_CONTENT_TYPE(txt, TXT)
    TRY_SET_CONTENT_TYPE(mp4, MP4);
  }

  // FIXME: replace string with StringPiece
  void addHeader(const string &key, const string &value) { headers_[key] = value; }

  // this function is re-writen using fmt and coring buffer...
  void appendToBuffer(flex_buffer *output) const {
    auto out = std::back_inserter(*output);
    // we can't support 1.1 since pipelining is tricky well beyond my time now.
    fmt::format_to(out, "HTTP/1.0 {} {}\r\n", statusCode_, statusMessage_);
    if (closeConnection_) {
      fmt::format_to(out, "Connection: close\r\n");
    } else {
      fmt::format_to(out, "Connection: Keep-Alive\r\n");
    }
    for (const auto &header : headers_) {
      fmt::format_to(out, "{}: {}\r\n", header.first, header.second);
    }
    fmt::format_to(out, "\r\n");
  }

 private:
  std::map<string, string> headers_;
  HttpStatusCode statusCode_;
  // FIXME: add http version
  string statusMessage_;
  bool closeConnection_;
};
}  // namespace coring::http
#endif  // CORING_HTTP_RESPONSE_HPP
