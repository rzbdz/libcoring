
#include "endpoint.hpp"
thread_local char coring::net::endpoint::local_resolve_buffer[64 * 1024];
// sad story, I am just debugging the DNS resolver
// I do know they are automatically static, just for emphasize.
static char digits[] = {'9', '8', '7', '6', '5', '4', '3', '2', '1', '0', '1', '2', '3', '4',
                        '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', '0', '1', '2',
                        '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
static const char *zero = digits + 9;
static void itoad(char buf[], int val) {
  int i = val;
  char *p = buf;
  do {
    int lsd = i % 10;
    i /= 10;
    *p++ = zero[lsd];
  } while (i != 0);
  if (val < 0) *p++ = '-';
  *p = '\0';
  std::reverse(buf, p);
}
void itoa2(char buf[], int val, int rdx, int cap) {
  const char *zero_ = cap ? zero : digits + 25;
  auto i = static_cast<unsigned>(val);
  char *p = buf;
  do {
    unsigned lsd = i % rdx;
    i /= rdx;
    *p++ = zero_[lsd];
  } while (i != 0);
  *p = '\0';
  std::reverse(buf, p);
}

/// TODO: this is very slow...
/// \param ipv4_int
/// \return
std::string int_to_ipv4(uint ipv4_int) {
  std::string ip_strs[4];
  for (int i = 0; i < 4; i++) {
    uint pos = i * 8;
    uint andi = ipv4_int & (255 << pos);
    char buf[4];
    itoad(buf, andi >> pos);
    ip_strs[i] = std::string(buf);
  }
  return ip_strs[0] + "." + ip_strs[1] + "." + ip_strs[2] + "." + ip_strs[3];
}

/// Warning: no port is specific, if you need to do HTTP/HTTPS stuff, just
/// don't forget to set the PORT number !!!
/// \param hostname
/// \param out
/// \return
bool coring::net::endpoint::resolve(const std::string &hostname, coring::net::endpoint *out) {
  struct hostent hent {};
  struct hostent *he = nullptr;
  int herrno = 0;
  ::memset(&hent, 0, sizeof(hent));
  // signal safe
  int ret = gethostbyname_r(hostname.c_str(), &hent, local_resolve_buffer, sizeof(local_resolve_buffer), &he, &herrno);
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

coring::net::endpoint coring::net::endpoint::from_resolve(const std::string &hostname) {
  endpoint res{};
  if (!resolve(hostname, &res)) {
    throw std::runtime_error("address resolve fails");
  }
  // TODO: a bad solution (or I should use FIXME here)
  res.set_port(net::host_to_network(80));
  return res;
}

coring::net::endpoint coring::net::endpoint::from_resolve(const std::string &hostname, int port) {
  endpoint res{};
  if (!resolve(hostname, &res)) {
    throw std::runtime_error("address resolve fails");
  }
  res.set_port(net::host_to_network(port));
  return res;
}
