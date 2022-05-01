// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef CORING_HTTP_CONTEXT_HPP
#define CORING_HTTP_CONTEXT_HPP
#include "http_request.hpp"
#include "coring/buffer.hpp"

namespace coring::http {
class HttpContext : public coring::copyable {
 public:
  enum HttpRequestParseState {
    kExpectRequestLine,
    kExpectHeaders,
    kExpectBody,
    kGotAll,
  };

  HttpContext() : state_(kExpectRequestLine) {}

  // default copy-ctor, dtor and assignment are fine

  // return false if any error
  bool parseRequest(flex_buffer *buf, timestamp receiveTime) {
    bool ok = true;
    bool hasMore = true;
    while (hasMore) {
      // LOG_TRACE("inf in parse request");
      if (state_ == kExpectRequestLine) {
        const char *crlf = buf->find_crlf();
        if (crlf) {
          ok = processRequestLine(buf->front(), crlf);
          if (ok) {
            request_.setReceiveTime(receiveTime);
            buf->has_read((crlf + 2) - buf->front());
            state_ = kExpectHeaders;
          } else {
            hasMore = false;
          }
        } else {
          hasMore = false;
        }
      } else if (state_ == kExpectHeaders) {
        const char *crlf = buf->find_crlf();
        const char *begin = buf->front();
        if (crlf) {
          const char *colon = std::find(begin, crlf, ':');
          if (colon != crlf) {
            request_.addHeader(begin, colon, crlf);
          } else {
            // empty line, end of header
            // FIXME:
            state_ = kGotAll;
            hasMore = false;
          }
          buf->has_read((crlf + 2) - buf->front());
        } else {
          hasMore = false;
        }
      } else if (state_ == kExpectBody) {
        // FIXME:
      }
    }
    return ok;
  }

  bool gotAll() const { return state_ == kGotAll; }

  void reset() {
    state_ = kExpectRequestLine;
    HttpRequest dummy;
    request_.swap(dummy);
  }

  const HttpRequest &request() const { return request_; }

  HttpRequest &request() { return request_; }

 private:
  bool processRequestLine(const char *begin, const char *end) {
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end && request_.setMethod(start, space)) {
      start = space + 1;
      space = std::find(start, end, ' ');
      if (space != end) {
        const char *question = std::find(start, space, '?');
        if (question != space) {
          request_.setPath(start, question);
          request_.setQuery(question, space);
        } else {
          request_.setPath(start, space);
        }
        start = space + 1;
        succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
        if (succeed) {
          if (*(end - 1) == '1') {
            request_.setVersion(HttpRequest::kHttp11);
          } else if (*(end - 1) == '0') {
            request_.setVersion(HttpRequest::kHttp10);
          } else {
            succeed = false;
          }
        }
      }
    }
    return succeed;
  }

  HttpRequestParseState state_;
  HttpRequest request_;
};
}  // namespace coring::http
#endif  // CORING_HTTP_CONTEXT_HPP
