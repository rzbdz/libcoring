
#include "endpoint.hpp"
thread_local char coring::net::endpoint::local_resolve_buffer[64 * 1024];
bool coring::net::endpoint::resolve(const std::string &hostname, coring::net::endpoint *out) {
  struct hostent hent {};
  struct hostent *he = nullptr;
  int herrno = 0;
  ::memset(&hent, 0, sizeof(hent));
  // signal safe
  int ret = gethostbyname_r(hostname.c_str(), &hent, local_resolve_buffer, sizeof(local_resolve_buffer), &he, &herrno);
  if (ret == 0 && he != nullptr) {
    assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
    // TODO: it will return multiple ip address, if one fails, we should retry others.
    // A solution is to copy the resolve buffer (it depends on how many we got)
    out->addr4_.sin_addr = *reinterpret_cast<struct in_addr *>(he->h_addr);
    return true;
  } else {
    // TODO+: check herrno
    return false;
  }
}
coring::net::endpoint coring::net::endpoint::from_resolve(const std::string &hostname) {
  endpoint res{};
  if (!resolve(hostname, &res)) {
    // TODO: error handling...
  }
  return res;
}
