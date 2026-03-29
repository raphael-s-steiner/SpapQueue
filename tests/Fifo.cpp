/*
Copyright 2026 Raphael S. Steiner

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

@author Raphael S. Steiner
*/

#include "ParallelPriorityQueue/LocalQueues/Fifo.hpp"

#include <gtest/gtest.h>

#include <numeric>

#include "ParallelPriorityQueue/Concepts/BasicQueue.hpp"

using namespace spapq;

TEST(FifoTest, BasicQueueConcept) {
    EXPECT_TRUE(BasicQueue<Fifo<unsigned>>);

    using IntLongPair = std::pair<int, long>;
    EXPECT_TRUE(BasicQueue<Fifo<IntLongPair>>);
}

TEST(FifoTest, Values1) {
    std::array<int, 5> values{8, 5, 2, 1, 34};

    Fifo<int> channel;
    EXPECT_TRUE(channel.empty());
    for (int val : values) {
        channel.push(val);
        EXPECT_FALSE(channel.empty());
    }
    EXPECT_EQ(channel.size(), values.size());

    for (int val : values) {
        EXPECT_FALSE(channel.empty());
        EXPECT_EQ(channel.top(), val);
        channel.pop();
    };
    EXPECT_TRUE(channel.empty());
}

TEST(FifoTest, Values2) {
    std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};

    Fifo<int> channel;
    EXPECT_TRUE(channel.empty());
    for (int val : values) {
        channel.push(val);
        EXPECT_FALSE(channel.empty());
    }
    EXPECT_EQ(channel.size(), values.size());

    for (int val : values) {
        EXPECT_FALSE(channel.empty());
        EXPECT_EQ(channel.top(), val);
        channel.pop();
    };
    EXPECT_TRUE(channel.empty());
}

TEST(FifoTest, Values3) {
    std::array<double, 5> values{0.23, -3.23, 23.934, 93.2902, -0.02};

    Fifo<double> channel;
    EXPECT_TRUE(channel.empty());
    for (double val : values) {
        channel.push(val);
        EXPECT_FALSE(channel.empty());
    }
    EXPECT_EQ(channel.size(), values.size());

    for (double val : values) {
        EXPECT_FALSE(channel.empty());
        EXPECT_EQ(channel.top(), val);
        channel.pop();
    };
    EXPECT_TRUE(channel.empty());
}

TEST(FifoTest, BatchPush) {
    constexpr std::size_t batch = 20U;
    constexpr std::size_t numIt = 50U;

    std::vector<int> values(numIt * batch);
    std::iota(values.begin(), values.end(), 27);

    Fifo<int> channel;
    EXPECT_TRUE(channel.empty());

    std::size_t cntr = 0U;
    for (auto it = values.cbegin(); it != values.cend();) {
        EXPECT_EQ(channel.size(), cntr);

        auto endIt = std::next(it, batch);
        channel.push(it, endIt);

        EXPECT_FALSE(channel.empty());
        cntr += batch;
        EXPECT_EQ(channel.size(), cntr);

        while (it != endIt) {
            int result = channel.top();
            EXPECT_EQ(*it, result);
            ++it;

            channel.pop();
            EXPECT_EQ(channel.size(), --cntr);
        }
    }
}
