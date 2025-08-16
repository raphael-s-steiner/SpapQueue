#include <gtest/gtest.h>

#include "ringBuffer/ringBuffer.hpp"

using namespace parPrioQ;

TEST(RingBufferTest, AlignAssertions) {

  RingBuffer<int, 4> channel1;
  channel1.push(4);
  const int &firstDatum1 = channel1.front();
  EXPECT_EQ(reinterpret_cast<std::size_t>(&firstDatum1) % CACHE_LINE_SIZE, 0);


  RingBuffer<char, 4> channel2;
  channel2.push('a');
  const char &firstDatum2 = channel2.front();
  EXPECT_EQ(reinterpret_cast<std::size_t>(&firstDatum2) % CACHE_LINE_SIZE, 0);


  RingBuffer<std::array<unsigned, 23>, 4> channel3;
  channel3.push({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  const std::array<unsigned, 23> &firstDatum3 = channel3.front();
  EXPECT_EQ(reinterpret_cast<std::size_t>(&firstDatum3) % CACHE_LINE_SIZE, 0);
}