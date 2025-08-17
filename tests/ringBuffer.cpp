#include <gtest/gtest.h>

#include <iostream>
#include <thread>

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


TEST(RingBufferTest, Values1) {

  std::array<int, 5> values{8, 5, 2, 1, 34};
  
  RingBuffer<int, 5> channel;
  for (int val : values) {
    channel.push(val);
  }
  for (int val : values) {
    EXPECT_EQ(channel.front(), val);
    channel.pop();
  };
}


TEST(RingBufferTest, Values2) {

  std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};
  
  RingBuffer<int, 5> channel;
  for (int val : values) {
    channel.push(val);
    EXPECT_EQ(channel.front(), val);
    channel.pop();
  }
}


TEST(RingBufferTest, Functionality1) {

  std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};
  
  RingBuffer<int, 5> channel;
  EXPECT_TRUE(channel.isEmpty());
  EXPECT_FALSE(channel.isFull());
  EXPECT_EQ(channel.occupancy(), 0U);
  EXPECT_EQ(channel.getCapacity(), 5);

  for (std::size_t i = 0U; i < values.size(); ++i) {
    int val = values[i];
    EXPECT_EQ(channel.occupancy(), std::min(i, static_cast<std::size_t>(5U)));
    bool success = channel.push(val);
    EXPECT_EQ(channel.occupancy(), std::min(i+1, static_cast<std::size_t>(5U)));
    EXPECT_EQ(success, i < 5);
  }

  EXPECT_TRUE(channel.isFull());
}

TEST(RingBufferTest, Functionality2) {

  std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};
  
  RingBuffer<int, 6> channel;
  EXPECT_TRUE(channel.isEmpty());
  EXPECT_FALSE(channel.isFull());
  EXPECT_EQ(channel.occupancy(), 0U);
  EXPECT_EQ(channel.getCapacity(), 6);

  for (std::size_t i = 0U; i < values.size(); ++i) {
    int val = values[i];
    EXPECT_EQ(channel.occupancy(), std::min(i, static_cast<std::size_t>(6U)));
    bool success = channel.push(val);
    EXPECT_EQ(channel.occupancy(), std::min(i+1, static_cast<std::size_t>(6U)));
    EXPECT_EQ(success, i < 6U);
  }

  EXPECT_TRUE(channel.isFull());

  for (std::size_t i = 0U; i < values.size(); ++i) {
    int val = values[i];

    if (i < 6U) {
      EXPECT_EQ(channel.occupancy(), 6U - i);
    } else {
      EXPECT_EQ(channel.occupancy(), 0U);
    }

    int front = channel.front();
    bool success = channel.pop();
    
    if (i < 6U) {
      EXPECT_EQ(channel.occupancy(), 5U - i);
      EXPECT_EQ(success, i < 6U);
      EXPECT_EQ(val, front);
    }
  }

  EXPECT_TRUE(channel.isEmpty());
}

TEST(RingBufferTest, Functionality3) {

  std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};
  
  RingBuffer<int, 6> channel;
  EXPECT_FALSE(channel.push(values.cbegin(), values.cend()));
  EXPECT_TRUE(channel.isEmpty());
  

  auto it = values.cbegin();
  std::advance(it, 6);
  EXPECT_TRUE(channel.push(values.cbegin(), it));

  EXPECT_TRUE(channel.pop());
  EXPECT_TRUE(channel.pop());
  EXPECT_TRUE(channel.pop());

  it = values.cbegin();
  std::advance(it, 4);
  EXPECT_FALSE(channel.push(values.cbegin(), it));

  EXPECT_TRUE(channel.pop());

  it = values.cbegin();
  std::advance(it, 4);
  EXPECT_TRUE(channel.push(values.cbegin(), it));
  EXPECT_TRUE(channel.isFull());

  for (int val : {-5, 123, 9, 23, 4, 1}) {
    EXPECT_EQ(val, channel.front());
    EXPECT_TRUE(channel.pop());
  }

  EXPECT_FALSE(channel.isFull());
  EXPECT_FALSE(channel.pop());
  EXPECT_TRUE(channel.isEmpty());
}

TEST(RingBufferTest, Multithread) {
  std::vector<int> values(100000);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = std::rand();
  }

  constexpr std::size_t capacity = 64;
  RingBuffer<int, capacity> channel;
  EXPECT_EQ(capacity, channel.getCapacity());

  std::thread consumer(
    [&channel, &values]() {
      for (std::size_t i = 0; i < values.size(); ++i) {
        while (channel.isEmpty()) {}
        EXPECT_FALSE(channel.isEmpty());
        EXPECT_LE(0U, channel.occupancy());
        EXPECT_LE(channel.occupancy(), channel.getCapacity());
        int val = channel.front();
        EXPECT_EQ(val, values[i]);
        EXPECT_TRUE(channel.pop());
      }
      EXPECT_TRUE(channel.isEmpty());
    }
  );

  std::thread producer(
    [&channel, &values]() {
      for (std::size_t i = 0; i < values.size(); ++i) {
        while (channel.isFull()) {}
        EXPECT_FALSE(channel.isFull());
        EXPECT_LE(0U, channel.occupancy());
        EXPECT_LE(channel.occupancy(), channel.getCapacity());
        EXPECT_TRUE(channel.push(values[i]));
      }
    }
  );

  producer.join();
  consumer.join();

  EXPECT_TRUE(channel.isEmpty());
}