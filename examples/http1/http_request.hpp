// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#ifndef CORING_HTTP_REQUEST_HPP
#define CORING_HTTP_REQUEST_HPP
#include <string>
#include <map>
#include "coring/detail/copyable.hpp"
#include "coring/timestamp.hpp"
using coring::timestamp;
using std::string;
namespace coring::http {
class HttpRequest : public ::coring::copyable {
 public:
  enum Method { kInvalid, kGet, kPost, kHead, kPut, kDelete };
  enum Version { kUnknown, kHttp10, kHttp11 };

  HttpRequest() : method_(kInvalid), version_(kUnknown) {}

  void setVersion(Version v) { version_ = v; }

  Version getVersion() const { return version_; }

  bool setMethod(const char *start, const char *end) {
    string m(start, end);
    if (m == "GET") {
      method_ = kGet;
    } else if (m == "POST") {
      method_ = kPost;
    } else if (m == "HEAD") {
      method_ = kHead;
    } else if (m == "PUT") {
      method_ = kPut;
    } else if (m == "DELETE") {
      method_ = kDelete;
    } else {
      method_ = kInvalid;
    }
    return method_ != kInvalid;
  }

  Method method() const { return method_; }

  const char *methodString() const {
    const char *result = "UNKNOWN";
    switch (method_) {
      case kGet:
        result = "GET";
        break;
      case kPost:
        result = "POST";
        break;
      case kHead:
        result = "HEAD";
        break;
      case kPut:
        result = "PUT";
        break;
      case kDelete:
        result = "DELETE";
        break;
      default:
        break;
    }
    return result;
  }

  void setPath(const char *start, const char *end) {
    path_.assign("public");
    path_.append(start, end - start);
    // LOG_TRACE("the req path is: {}", path_);
  }

  const string &path() const { return path_; }

  void setQuery(const char *start, const char *end) { query_.assign(start, end); }

  const string &query() const { return query_; }

  void setReceiveTime(timestamp t) { receiveTime_ = t; }

  timestamp receiveTime() const { return receiveTime_; }

  void addHeader(const char *start, const char *colon, const char *end) {
    string field(start, colon);
    ++colon;
    while (colon < end && isspace(*colon)) {
      ++colon;
    }
    string value(colon, end);
    while (!value.empty() && isspace(value[value.size() - 1])) {
      value.resize(value.size() - 1);
    }
    headers_[field] = value;
  }

  string getHeader(const string &field) const {
    string result;
    std::map<string, string>::const_iterator it = headers_.find(field);
    if (it != headers_.end()) {
      result = it->second;
    }
    return result;
  }

  const std::map<string, string> &headers() const { return headers_; }

  void swap(HttpRequest &that) {
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    receiveTime_.swap(that.receiveTime_);
    headers_.swap(that.headers_);
  }

 private:
  Method method_;
  Version version_;
  string path_;
  string query_;
  timestamp receiveTime_;
  std::map<string, string> headers_;
};

}  // namespace coring::http
#endif  // CORING_HTTP_REQUEST_HPP
