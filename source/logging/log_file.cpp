// log_file.cpp
// Created by PanJunzhong on 2022/4/28.
//
#include "coring/coring_config.hpp"
#include "coring/detail/logging/log_file.hpp"

const off_t coring::log_file::k_file_roll_size = LOG_FILE_ROLLING_SZ;

coring::log_file::log_file(std::string name, off_t roll_size) : name_{std::move(name)}, roll_size_{roll_size} {
#ifdef CORING_ASYNC_LOGGER_STDOUT
  char buf[] = "Async Logger started, using stdout as output, log name: ";
  fwrite(buf, 1, sizeof(buf), stdout);
  fwrite(name.c_str(), 1, name.size(), stdout);
  fwrite("\n", 1, 1, stdout);
#else
  roll_file();
#endif
}
void coring::log_file::roll_file() {
#ifdef CORING_ASYNC_LOGGER_STDOUT
// do nothing...
#else
  log_timestamp ts;
  char buf[40];
  char *ret = ts.format_date_to(buf) - 1;
  ret[0] = '.';
  ret[1] = 'l';
  ret[2] = 'o';
  ret[3] = 'g';
  ret[4] = '\0';
  name_.append(buf);
  os_ = std::ofstream{name_, std::ios::app | std::ios::out};
#endif
}
void coring::log_file::append(char *data, std::streamsize len) {
#ifdef CORING_ASYNC_LOGGER_STDOUT
  fwrite(data, 1, len, stdout);
#else
  // Getting the current write position.
  if (os_.tellp() >= roll_size_) {
    os_.flush();
    roll_file();
  }
  os_.write(data, len);
#endif
}
void coring::log_file::force_flush() {
#ifdef CORING_ASYNC_LOGGER_STDOUT
  fflush(stdout);
#else
  os_.flush();
#endif
}