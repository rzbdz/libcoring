
#ifndef CORING_ENDIAN_HPP
#define CORING_ENDIAN_HPP
#include <cstdint>
#include <endian.h>
namespace coring {

inline uint64_t host_to_network(uint64_t host64) { return htobe64(host64); }

inline uint32_t host_to_network(uint32_t host32) { return htobe32(host32); }

inline uint16_t host_to_network(uint16_t host16) { return htobe16(host16); }

inline uint64_t network_to_host(uint64_t net64) { return be64toh(net64); }

inline uint32_t network_to_host(uint32_t net32) { return be32toh(net32); }

inline uint16_t network_to_host(uint16_t net16) { return be16toh(net16); }

}  // namespace coring

#endif  // CORING_ENDIAN_HPP
