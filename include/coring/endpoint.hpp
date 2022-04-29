
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
#include "coring/detail/str_utils.hpp"

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

  /// Just make it clear that this is trivally copyable.
  endpoint(const endpoint &rhs) = default;
  endpoint(endpoint &&rhs) = default;
  endpoint &operator=(const endpoint &rhs) = default;
  endpoint &operator=(endpoint &&rhs) = default;

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
  inline static thread_local char local_resolve_buffer[64 * 1024];

  /// Get a string like: "127.0.0.1" (ipv4 only)
  std::string address_str() {
    uint ipv4_int = *reinterpret_cast<uint *>(&addr4_.sin_addr);
    std::string ip_strs[4];
    for (int i = 0; i < 4; i++) {
      uint pos = i * 8;
      uint and_v = ipv4_int & (255 << pos);
      char buf[4];
      ::coring::detail::itoad(buf, static_cast<int>(and_v) >> pos);
      ip_strs[i] = std::string(buf);
    }
    return ip_strs[0] + "." + ip_strs[1] + "." + ip_strs[2] + "." + ip_strs[3];
  }

  /// Get a string like: "127.0.0.1:80" (ipv4 only)
  std::string to_str() {
    uint ipv4_int = *reinterpret_cast<uint *>(&addr4_.sin_addr);
    std::string ip_strs[4];
    for (int i = 0; i < 4; i++) {
      uint pos = i * 8;
      uint and_v = ipv4_int & (255 << pos);
      char buf[4];
      ::coring::detail::itoad(buf, static_cast<int>(and_v) >> pos);
      ip_strs[i] = std::string(buf);
    }
    char port_buf[10];
    port_buf[0] = ':';
    ::coring::detail::itoad(port_buf, static_cast<int>(network_to_host(addr4_.sin_port)));
    auto p = std::string(port_buf);
    return ip_strs[0] + "." + ip_strs[1] + "." + ip_strs[2] + "." + ip_strs[3] + p;
  }

  static bool resolve(const std::string &hostname, endpoint *out) {
    struct hostent hent {};
    struct hostent *he = nullptr;
    int herrno = 0;
    ::memset(&hent, 0, sizeof(hent));
    // signal safe
    int ret =
        gethostbyname_r(hostname.c_str(), &hent, local_resolve_buffer, sizeof(local_resolve_buffer), &he, &herrno);
    if (ret == 0 && he != nullptr) {
      //    std::cout << (*reinterpret_cast<uint *>(he->h_addr)) << std::endl;
      //    std::cout << int_to_ipv4((*reinterpret_cast<uint *>(he->h_addr))) << std::endl;
      //    std::cout << he->h_aliases[0] << std::endl;
      assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
      // TODO: it will return multiple ip address, if one fails, we should retry others.
      // A solution is to copy the resolve buffer(it depends on how many we got)
      out->addr4_.sin_family = AF_INET;
      out->addr4_.sin_addr = *reinterpret_cast<struct in_addr *>(he->h_addr);
      return true;
    } else {
      // TODO+: check herrno
      return false;
    }
  }
  /// since the port isn't provided, we set it to 80 by default
  /// \param hostname sth like "www.xxxx.com"
  /// \return a endpoint with www.xxxx.com:80
  static endpoint from_resolve(const std::string &hostname) {
    endpoint res{};
    if (!resolve(hostname, &res)) {
      throw std::runtime_error("address resolve fails");
    }
    // TODO: a bad solution (or I should use FIXME here)
    res.set_port(net::host_to_network(80));
    return res;
  }
  ///
  /// \param hostname sth like "www.xxxx.com"
  /// \param port a uint16 port number in [0, 65536)
  /// \return
  static endpoint from_resolve(const std::string &hostname, uint16_t port) {
    endpoint res{};
    if (!resolve(hostname, &res)) {
      throw std::runtime_error("address resolve fails");
    }
    res.set_port(net::host_to_network(port));
    return res;
  }

  [[nodiscard]] sa_family_t family() const { return addr4_.sin_family; }
  [[nodiscard]] uint16_t port() const { return addr4_.sin_port; }
  /// make sure you pass by a network endian
  void set_port(uint16_t p) { addr4_.sin_port = p; }
  [[nodiscard]] auto as_sockaddr() { return detail::sockaddr_cast(&addr4_); }
  [[nodiscard]] auto as_sockaddr() const { return detail::sockaddr_cast(&addr4_); }
  [[nodiscard]] auto as_sockaddr_in() { return &addr4_; }

 private:
  ::sockaddr_in addr4_;
};
typedef endpoint endpoint_v4;
}  // namespace coring::net

#endif  // CORING_ENDPOINT_HPP
