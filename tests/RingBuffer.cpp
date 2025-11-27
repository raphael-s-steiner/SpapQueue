#include "RingBuffer/RingBuffer.hpp"

#include <gtest/gtest.h>

#include <thread>

using namespace spapq;

TEST(RingBufferTest, Values1) {
    std::array<int, 5> values{8, 5, 2, 1, 34};

    RingBuffer<int, 5> channel;
    for (int val : values) {
        bool succ = channel.push(val);
        EXPECT_TRUE(succ);
    }
    for (int val : values) { EXPECT_EQ(channel.pop().value(), val); };
}

TEST(RingBufferTest, Values2) {
    std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};

    RingBuffer<int, 5> channel;
    for (int val : values) {
        bool succ = channel.push(val);
        EXPECT_TRUE(succ);
        EXPECT_EQ(channel.pop().value(), val);
    }
}

TEST(RingBufferTest, Values3) {
    std::array<std::string, 5> values{"Hello World!", "", "Elephant", "312", "All done!"};

    RingBuffer<std::string, 5> channel;
    for (auto val : values) {
        bool succ = channel.push(val);
        EXPECT_TRUE(succ);
    }
    for (auto val : values) { EXPECT_EQ(channel.pop().value(), val); };
}

TEST(RingBufferTest, Functionality1) {
    std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};

    RingBuffer<int, 5> channel;
    EXPECT_TRUE(channel.isEmpty());
    EXPECT_FALSE(channel.isFull());
    EXPECT_EQ(channel.occupancy(), 0U);
    EXPECT_EQ(channel.getCapacity(), 5U);

    for (std::size_t i = 0U; i < values.size(); ++i) {
        int val = values[i];
        EXPECT_EQ(channel.occupancy(), std::min(i, static_cast<std::size_t>(5U)));
        bool success = channel.push(val);
        EXPECT_EQ(channel.occupancy(), std::min(i + 1, static_cast<std::size_t>(5U)));
        EXPECT_EQ(success, i < 5U);
    }

    EXPECT_TRUE(channel.isFull());
}

TEST(RingBufferTest, Functionality2) {
    std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};

    RingBuffer<int, 6> channel;
    EXPECT_TRUE(channel.isEmpty());
    EXPECT_FALSE(channel.isFull());
    EXPECT_EQ(channel.occupancy(), 0U);
    EXPECT_EQ(channel.getCapacity(), 6U);

    for (std::size_t i = 0U; i < values.size(); ++i) {
        int val = values[i];
        EXPECT_EQ(channel.occupancy(), std::min(i, static_cast<std::size_t>(6U)));
        bool success = channel.push(val);
        EXPECT_EQ(channel.occupancy(), std::min(i + 1, static_cast<std::size_t>(6U)));
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

        std::optional<int> result = channel.pop();
        bool success = result.has_value();
        EXPECT_EQ(success, i < 6U);

        if (i < 6U) {
            int front = result.value();
            EXPECT_EQ(channel.occupancy(), 5U - i);
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
        std::optional<int> result = channel.pop();
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(val, result.value());
    }

    EXPECT_FALSE(channel.isFull());
    EXPECT_FALSE(channel.pop());
    EXPECT_TRUE(channel.isEmpty());
}

TEST(RingBufferTest, Multithread1) {
    std::vector<int> values(100000);
    for (std::size_t i = 0U; i < values.size(); ++i) { values[i] = std::rand(); }

    constexpr std::size_t capacity = 64;
    RingBuffer<int, capacity> channel;
    EXPECT_EQ(capacity, channel.getCapacity());

    std::jthread consumer([&channel, &values]() {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            while (channel.isEmpty()) { }
            EXPECT_FALSE(channel.isEmpty());
            EXPECT_LE(0U, channel.occupancy());
            EXPECT_LE(channel.occupancy(), channel.getCapacity());
            std::optional<int> result = channel.pop();
            EXPECT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), values[i]);
        }
        EXPECT_TRUE(channel.isEmpty());
    });

    std::jthread producer([&channel, &values]() {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            while (channel.isFull()) { }
            EXPECT_FALSE(channel.isFull());
            EXPECT_LE(0U, channel.occupancy());
            EXPECT_LE(channel.occupancy(), channel.getCapacity());
            EXPECT_TRUE(channel.push(values[i]));
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(channel.isEmpty());
}

TEST(RingBufferTest, Multithread2) {
    std::vector<long> values(1000000);
    for (std::size_t i = 0; i < values.size(); ++i) { values[i] = std::rand(); }

    constexpr std::size_t capacity = 16U;
    RingBuffer<long, capacity> channel;
    EXPECT_EQ(capacity, channel.getCapacity());

    std::jthread consumer([&channel, &values]() {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            while (channel.isEmpty()) { }
            EXPECT_FALSE(channel.isEmpty());
            EXPECT_LE(0U, channel.occupancy());
            EXPECT_LE(channel.occupancy(), channel.getCapacity());
            std::optional<long> result = channel.pop();
            EXPECT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), values[i]);
        }
        EXPECT_TRUE(channel.isEmpty());
    });

    std::jthread producer([&channel, &values]() {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            while (!channel.push(values[i])) { }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(channel.isEmpty());
}

TEST(RingBufferTest, Multithread3) {
    std::vector<long> values(1000000);
    for (std::size_t i = 0; i < values.size(); ++i) { values[i] = std::rand(); }

    constexpr std::size_t capacity = 16U;
    RingBuffer<long, capacity> channel;
    EXPECT_EQ(capacity, channel.getCapacity());

    std::jthread consumer([&channel, &values]() {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            std::optional<long> popVal(std::nullopt);
            while (!popVal.has_value()) { popVal = channel.pop(); }
            EXPECT_LE(0U, channel.occupancy());
            EXPECT_LE(channel.occupancy(), channel.getCapacity());
            EXPECT_EQ(popVal.value(), values[i]);
        }
        EXPECT_TRUE(channel.isEmpty());
    });

    std::jthread producer([&channel, &values]() {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            while (!channel.push(values[i])) { }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(channel.isEmpty());
}

TEST(RingBufferTest, Multithread4) {
    std::vector<long> values(1000000);
    for (std::size_t i = 0; i < values.size(); ++i) { values[i] = std::rand(); }

    constexpr std::size_t capacity = 16U;
    RingBuffer<long, capacity> channel;
    EXPECT_EQ(capacity, channel.getCapacity());

    std::jthread consumer([&channel, &values]() {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            std::optional<long> popVal(std::nullopt);
            while (not(popVal = channel.pop())) { }
            EXPECT_LE(0U, channel.occupancy());
            EXPECT_LE(channel.occupancy(), channel.getCapacity());
            EXPECT_EQ(popVal.value(), values[i]);
        }
        EXPECT_TRUE(channel.isEmpty());
    });

    std::jthread producer([&channel, &values]() {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            while (!channel.push(values[i])) { }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(channel.isEmpty());
}

TEST(RingBufferTest, Alignment) {
    RingBuffer<int, 5> channel1;
    EXPECT_EQ(alignof(RingBuffer<int, 5>) % CACHE_LINE_SIZE, 0U);
    EXPECT_EQ(sizeof(channel1) % CACHE_LINE_SIZE, 0U);

    RingBuffer<char, 125> channel2;
    EXPECT_EQ(alignof(RingBuffer<char, 125>) % CACHE_LINE_SIZE, 0U);
    EXPECT_EQ(sizeof(channel2) % CACHE_LINE_SIZE, 0U);
}
