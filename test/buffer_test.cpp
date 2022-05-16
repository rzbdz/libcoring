#include "coring/detail/debug.hpp"
#include "coring/buffer.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>

class CORING_TEST_CLASS {
 public:
  static constexpr int scale = 65536;

  static void test_meta_bf() {
    coring::buffer bf{1024};
    assert(bf.capacity() == 1024);
    assert(bf.readable() == 0);
    assert(bf.writable() == 1024);
    int32_t test = 0x78654321;
    bf.push_back_int(static_cast<uint32_t>(test));
    ASSERT_EQ(bf.capacity(), 1024);
    EXPECT_EQ(bf.index_read_, 0);
    EXPECT_EQ(bf.index_write_, 4);
    EXPECT_EQ(bf.readable(), 4);
    EXPECT_EQ(bf.writable(), 1020);
    auto top_half = bf.pop_int<uint16_t>();
    EXPECT_EQ(top_half, 0x7865);
    auto bottom_half = bf.pop_int<uint16_t>();
    EXPECT_EQ(bottom_half, 0x4321);
    EXPECT_EQ(bf.index_read_, 0);
    EXPECT_EQ(bf.index_write_, 0);
    EXPECT_EQ(bf.readable(), 0);
    EXPECT_EQ(bf.writable(), 1024);
  }
  static void test_scale_bf() {
    coring::buffer bf{};
    char big_buf[scale];
    for (int i = 0; i < scale; i++) {
      big_buf[i] = static_cast<uint8_t>(i & 0xff);
    }

    for (int i = 0; i < scale;) {
      uint64_t u64 = *reinterpret_cast<uint64_t *>(&big_buf[i]);
      bf.push_back_int(u64);
      ASSERT_EQ((*reinterpret_cast<const uint64_t *>(bf.back() - 8)), coring::net::host_to_network(u64));
      i += 8;
      uint32_t u32 = *reinterpret_cast<uint32_t *>(&big_buf[i]);
      bf.push_back_int(u32);
      ASSERT_EQ((*reinterpret_cast<const uint32_t *>(bf.back() - 4)), coring::net::host_to_network(u32));
      i += 4;
      uint16_t u16 = *reinterpret_cast<uint16_t *>(&big_buf[i]);
      bf.push_back_int(u16);
      ASSERT_EQ((*reinterpret_cast<const uint16_t *>(bf.back() - 2)), coring::net::host_to_network(u16));
      i += 2;
      uint8_t u8 = *reinterpret_cast<uint8_t *>(&big_buf[i]);
      bf.push_back_int(u8);
      ASSERT_EQ((*reinterpret_cast<const uint8_t *>(bf.back() - 1)), coring::net::host_to_network(u8));
      i += 1;
      u8 = *reinterpret_cast<uint8_t *>(&big_buf[i]);
      bf.push_back_int(u8);
      ASSERT_EQ((*reinterpret_cast<const uint8_t *>(bf.back() - 1)), coring::net::host_to_network(u8));
      i += 1;
    }
    for (int i = 0; i < scale;) {
      uint64_t u64 = *reinterpret_cast<uint64_t *>(&big_buf[i]);
      auto verify64 = bf.pop_int<uint64_t>();
      ASSERT_EQ(verify64, u64);
      i += 8;
      uint32_t u32 = *reinterpret_cast<uint32_t *>(&big_buf[i]);
      auto verify32 = bf.pop_int<uint32_t>();
      ASSERT_EQ(verify32, u32);
      i += 4;
      uint16_t u16 = *reinterpret_cast<uint16_t *>(&big_buf[i]);
      auto verify16 = bf.pop_int<uint16_t>();
      ASSERT_EQ(verify16, u16);
      i += 2;
      uint8_t u8 = *reinterpret_cast<uint8_t *>(&big_buf[i]);
      auto verify8 = bf.pop_int<uint8_t>();
      ASSERT_EQ(verify8, u8);
      i += 1;
      u8 = *reinterpret_cast<uint8_t *>(&big_buf[i]);
      verify8 = bf.pop_int<uint8_t>();
      ASSERT_EQ(verify8, u8);
      i += 1;
    }
  }

  static void test_meta_fix() {
    std::vector<char> ct(1024);
    coring::fixed_buffer bf{ct.data(), 1024};
    assert(bf.capacity() == 1024);
    assert(bf.readable() == 0);
    assert(bf.writable() == 1024);
    uint32_t test = 0x78654321;
    // bf.push_back_int(static_cast<uint32_t>(test));
    *(reinterpret_cast<uint32_t *>(bf.back())) = coring::net::network_to_host(test);
    bf.has_written(4);
    ASSERT_EQ(bf.capacity(), 1024);
    EXPECT_EQ(bf.index_read_, 0);
    EXPECT_EQ(bf.index_write_, 4);
    EXPECT_EQ(bf.readable(), 4);
    EXPECT_EQ(bf.writable(), 1020);
    auto top_half = bf.pop_int<uint16_t>();
    EXPECT_EQ(top_half, 0x7865);
    auto bottom_half = bf.pop_int<uint16_t>();
    EXPECT_EQ(bottom_half, 0x4321);
    EXPECT_EQ(bf.index_read_, 0);
    EXPECT_EQ(bf.index_write_, 0);
    EXPECT_EQ(bf.readable(), 0);
    EXPECT_EQ(bf.writable(), 1024);
  }
  static void test_scale_fix() {
    std::vector<char> ct(65536);
    coring::fixed_buffer bf{ct.data(), 65536};
    char big_buf[scale];
    for (int i = 0; i < scale; i++) {
      big_buf[i] = static_cast<uint8_t>(i & 0xff);
    }

    for (int i = 0; i < scale;) {
      uint64_t u64 = *reinterpret_cast<uint64_t *>(&big_buf[i]);
      // bf.push_back_int(static_cast<uint32_t>(test));
      *(reinterpret_cast<uint64_t *>(bf.back())) = coring::net::host_to_network(u64);
      bf.has_written(8);
      ASSERT_EQ((*reinterpret_cast<const uint64_t *>(bf.back() - 8)), coring::net::host_to_network(u64));
      i += 8;
      uint32_t u32 = *reinterpret_cast<uint32_t *>(&big_buf[i]);
      *(reinterpret_cast<uint32_t *>(bf.back())) = coring::net::host_to_network(u32);
      bf.has_written(4);
      ASSERT_EQ((*reinterpret_cast<const uint32_t *>(bf.back() - 4)), coring::net::host_to_network(u32));
      i += 4;
      uint16_t u16 = *reinterpret_cast<uint16_t *>(&big_buf[i]);
      *(reinterpret_cast<uint16_t *>(bf.back())) = coring::net::host_to_network(u16);
      bf.has_written(2);
      ASSERT_EQ((*reinterpret_cast<const uint16_t *>(bf.back() - 2)), coring::net::host_to_network(u16));
      i += 2;
      uint8_t u8 = *reinterpret_cast<uint8_t *>(&big_buf[i]);
      *(reinterpret_cast<uint16_t *>(bf.back())) = coring::net::host_to_network(u8);
      bf.has_written(1);
      ASSERT_EQ((*reinterpret_cast<const uint8_t *>(bf.back() - 1)), coring::net::host_to_network(u8));
      i += 1;
      u8 = *reinterpret_cast<uint8_t *>(&big_buf[i]);
      *(reinterpret_cast<uint16_t *>(bf.back())) = coring::net::host_to_network(u8);
      bf.has_written(1);
      ASSERT_EQ((*reinterpret_cast<const uint8_t *>(bf.back() - 1)), coring::net::host_to_network(u8));
      i += 1;
    }
    for (int i = 0; i < scale;) {
      uint64_t u64 = *reinterpret_cast<uint64_t *>(&big_buf[i]);
      auto verify64 = bf.pop_int<uint64_t>();
      ASSERT_EQ(verify64, u64);
      i += 8;
      uint32_t u32 = *reinterpret_cast<uint32_t *>(&big_buf[i]);
      auto verify32 = bf.pop_int<uint32_t>();
      ASSERT_EQ(verify32, u32);
      i += 4;
      uint16_t u16 = *reinterpret_cast<uint16_t *>(&big_buf[i]);
      auto verify16 = bf.pop_int<uint16_t>();
      ASSERT_EQ(verify16, u16);
      i += 2;
      uint8_t u8 = *reinterpret_cast<uint8_t *>(&big_buf[i]);
      auto verify8 = bf.pop_int<uint8_t>();
      ASSERT_EQ(verify8, u8);
      i += 1;
      u8 = *reinterpret_cast<uint8_t *>(&big_buf[i]);
      verify8 = bf.pop_int<uint8_t>();
      ASSERT_EQ(verify8, u8);
      i += 1;
    }
  }
};

TEST(Buffer, TestFlexBufferSimple) { CORING_TEST_CLASS::test_meta_bf(); }

TEST(Buffer, TestFlexBufferScale) { CORING_TEST_CLASS::test_scale_bf(); }

TEST(Buffer, TestFixBufferSimple) { CORING_TEST_CLASS::test_meta_fix(); }

TEST(Buffer, TestFixBufferScale) { CORING_TEST_CLASS::test_meta_fix(); }