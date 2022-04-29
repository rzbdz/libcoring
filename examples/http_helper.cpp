// http_helper.cpp
// Created by PanJunzhong on 2022/4/26.
//
#include "http_helper.hpp"

#define SERVER_STRING "Server: Powered By `libcoring-v0.1` copycat/0.1\r\n"

void strtolower(char *str) {
  for (; *str; ++str) *str = (char)::tolower(*str);
}

char digits[] = {'9', '8', '7', '6', '5', '4', '3', '2', '1', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
const char *zero = digits + 9;

int prepare_content_length_text(char buf[], long val) {
  long i = val;
  char *p = buf;
  int j = 0;
  do {
    j++;
    long lsd = i % 10;
    i /= 10;
    *p++ = zero[lsd];
  } while (i != 0);
  if (val < 0) {
    j++;
    *p++ = '-';
  }
  *p = '\0';
  std::reverse(buf, p);
  // When the browser sees a '\r\n' sequence in a line on its own,
  // it understands there are no more headers. Content may follow.
  *p++ = '\r';
  *p++ = '\n';
  *p++ = '\r';
  *p++ = '\n';
  *p = '\0';
  return j + 4;
}

http_req_line parse_http_method(char *request_line) {
  // GET URL HTTP/1.0
  // TODO: add supports for normal http request
  char *method, *path, *saveptr;
  method = strtok_r(request_line, " ", &saveptr);
  strtolower(method);
  path = strtok_r(NULL, " ", &saveptr);
  if (strcmp(method, "get") == 0) {
    // Sad story, it seems strtok_r doesn't provide a good saveptr semantic...
    // there would be a O(n) strlen call in string_view construction
    //  TODO: we should write our own strtok_r...
    return {GET, {path}, v10};
  } else {
    return {};
  }
}

const char *get_filename_ext(const char *filename) {
  const char *dot = ::strrchr(filename, '.');
  if (!dot || dot == filename) return "";
  return dot + 1;
}

#define GEN_CONTENT_TYPE(valname, type)        \
  auto valname = "Content-Type: " type "\r\n"; \
  static_cast<void>(0)

#define TRY_EMPLACE_CONTENT_TYPE(suffix, type_str)                                  \
  if (strcmp(#suffix, file_ext) == 0) buf.emplace_back(type_str, strlen(type_str)); \
  static_cast<void>(0)

void prepare_http_headers(coring::flex_buffer &buf, const char *req_path, off_t content_length) {
  char small_case_path[1024];
  strcpy(small_case_path, req_path);
  strtolower(small_case_path);
  const char *str = "HTTP/1.0 200 OK\r\n";
  buf.emplace_back(str, strlen(str));
  buf.emplace_back(SERVER_STRING, strlen(SERVER_STRING));
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
  TRY_EMPLACE_CONTENT_TYPE(jpeg, JPEG);
  TRY_EMPLACE_CONTENT_TYPE(png, PNG);
  TRY_EMPLACE_CONTENT_TYPE(jpg, JPEG);
  TRY_EMPLACE_CONTENT_TYPE(gif, GIF);
  TRY_EMPLACE_CONTENT_TYPE(htm, HTML);
  TRY_EMPLACE_CONTENT_TYPE(html, HTML);
  TRY_EMPLACE_CONTENT_TYPE(js, JS);
  TRY_EMPLACE_CONTENT_TYPE(css, CSS);
  TRY_EMPLACE_CONTENT_TYPE(txt, TXT);
  TRY_EMPLACE_CONTENT_TYPE(mp4, MP4);
  char content_len_buf[26] = "content-length: ";
  // 10 digits could represent 0 ~ 9 999 999 999, which is almost 10 billion bytes
  // we need to do a reverse operation, extra buffer is needed.
  int len = prepare_content_length_text(content_len_buf + 16, content_length);
  // Send the content-length header, which is the file size in this case.
  buf.emplace_back(content_len_buf, 16 + len);
}

void prepare_filename(char *buf, std::string_view &url) {
  // go for a default setting if no explicit filename
  if (url[url.size() - 1] == '/') {
    strcpy(buf, "public");
    strcat(buf, url.data());
    strcat(buf, "index.html");
  } else {
    // go for file
    strcpy(buf, "public");
    strcat(buf, url.data());
  }
}