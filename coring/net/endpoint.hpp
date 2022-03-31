
#ifndef CORING_ENDPOINT_HPP
#define CORING_ENDPOINT_HPP
#pragma once
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <type_traits>
#include <string>
#include <stdexcept>
#include <ranges>
#include <vector>
#include <cassert>
#include "endian.hpp"
namespace coring::net {
namespace detail {
template <typename SockAddrIn>
const struct sockaddr *sockaddr_cast(const SockAddrIn *addr) {
  static_assert(std::is_same_v<sockaddr_in, SockAddrIn> || std::is_same_v<sockaddr_in6, SockAddrIn>);
  return reinterpret_cast<const struct sockaddr *>(addr);
}
template <typename SockAddrIn>
struct sockaddr *sockaddr_cast(SockAddrIn *addr) {
  static_assert(std::is_same_v<sockaddr_in, SockAddrIn> || std::is_same_v<sockaddr_in6, SockAddrIn>);
  return reinterpret_cast<struct sockaddr *>(addr);
}
}  // namespace detail
/// based on sockaddr, support both udp and tcp.
/// TODO: support ipv6...
class endpoint {
 public:
  static constexpr socklen_t len = sizeof(sockaddr_in);
  explicit endpoint(uint16_t port = 0) noexcept
      : addr4_{.sin_family = AF_INET, .sin_port = net::host_to_network(port)} {}
  endpoint(const std::string &ip, uint16_t port) {
    addr4_.sin_family = AF_INET;
    addr4_.sin_port = net::host_to_network(port);
    if (::inet_pton(AF_INET, ip.c_str(), &addr4_.sin_addr) <= 0) {
      throw std::runtime_error("wrong ip address form");
    }
    // std::cout << *(reinterpret_cast<uint32_t *>(&addr4_.sin_addr)) << std::endl;
  }
  explicit endpoint(const std::string &ip_port) {
    auto split_ptr = ::strchr(ip_port.c_str(), ':');
    if (split_ptr == nullptr) {
      throw std::runtime_error("not a ip_port address");
    }
    auto port_num = static_cast<uint16_t>(::atoi(split_ptr));
    auto ip = ip_port.substr(0, split_ptr - ip_port.c_str());
    addr4_.sin_family = AF_INET;
    addr4_.sin_port = net::host_to_network(port_num);
    if (::inet_pton(AF_INET, ip.c_str(), &addr4_.sin_addr) <= 0) {
      throw std::runtime_error("wrong ip address form");
    }
  }
  // For MT-Safe gethostbyname_r
  static thread_local char local_resolve_buffer[64 * 1024];

  static bool resolve(const std::string &hostname, endpoint *out);
  static endpoint from_resolve(const std::string &hostname);
  static endpoint from_resolve(const std::string &hostname, int port);
  [[nodiscard]] sa_family_t family() const { return addr4_.sin_family; }
  [[nodiscard]] uint16_t port() const { return addr4_.sin_port; }
  /// make sure you pass by a network endian
  void set_port(uint16_t p) { addr4_.sin_port = p; }
  auto as_sockaddr() { return detail::sockaddr_cast(&addr4_); }
  auto as_sockaddr_in() { return &addr4_; }

 private:
  ::sockaddr_in addr4_;
};
}  // namespace coring::net

#endif  // CORING_ENDPOINT_HPP
