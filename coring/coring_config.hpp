//
// Created by A1134 on 2022/4/7.
//

#ifndef CORING_CORING_CONFIG_HPP
#define CORING_CORING_CONFIG_HPP
//  #define CORING_ASYNC_LOGGER_STDOUT
class CORING_TEST_CLASS;
namespace coring {
constexpr int BUFFER_DEFAULT_SIZE = 128;
constexpr int READ_BUFFER_AT_LEAST_WRITABLE = 128;
constexpr size_t ASYNC_LOGGER_MAX_BUFFER = 1000 * 4000;
constexpr size_t ASYNC_LOGGER_MAX_MESSAGE = 500;
constexpr size_t ASYNC_LOGGER_RING_BUFFER_SZ = 8192;
}  // namespace coring
#endif  // CORING_CORING_CONFIG_HPP
