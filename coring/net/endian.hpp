#ifndef CORING_ENDIAN_HPP
#define CORING_ENDIAN_HPP
#include <cstdint>
#include <endian.h>
namespace coring {
// I forgot uint8 before, sad. X<
template <typename other_t_>
inline other_t_ host_to_network(other_t_) = delete;

template <typename other_t_>
inline other_t_ network_to_host(other_t_) = delete;

// uint h -> n 4X
inline uint64_t host_to_network(uint64_t host64) { return htobe64(host64); }

inline uint32_t host_to_network(uint32_t host32) { return htobe32(host32); }

inline uint16_t host_to_network(uint16_t host16) { return htobe16(host16); }

inline uint8_t host_to_network(uint8_t host8) { return host8; }

// int h -> n 4X
inline int64_t host_to_network(int64_t host64) { return htobe64(host64); }

inline int32_t host_to_network(int32_t host32) { return htobe32(host32); }

inline int16_t host_to_network(int16_t host16) { return htobe16(host16); }

inline int8_t host_to_network(int8_t host8) { return host8; }

// uint n -> h 4X
inline uint64_t network_to_host(uint64_t net64) { return be64toh(net64); }

inline uint32_t network_to_host(uint32_t net32) { return be32toh(net32); }

inline uint16_t network_to_host(uint16_t net16) { return be16toh(net16); }

inline uint8_t network_to_host(uint8_t net8) { return net8; }

// int n-> h 4X
inline int64_t network_to_host(int64_t net64) { return be64toh(net64); }

inline int32_t network_to_host(int32_t net32) { return be32toh(net32); }

inline int16_t network_to_host(int16_t net16) { return be16toh(net16); }

inline int8_t network_to_host(int8_t net8) { return net8; }

}  // namespace coring
#endif  // CORING_ENDIAN_HPP