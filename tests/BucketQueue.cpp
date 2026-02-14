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

#include "ParallelPriotityQueue/LocalQueues/BucketQueue.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>

#include "ParallelPriotityQueue/Concepts/BasicQueue.hpp"

using namespace spapq;

TEST(BucketQueueTest, BasicQueueConcept) {
    EXPECT_TRUE(BasicQueue<BucketQueue<unsigned>>);

    using IntLongPair = std::pair<int, long>;
    EXPECT_TRUE(BasicQueue<BucketQueue<IntLongPair>>);
}

TEST(BucketQueueTest, Values1) {
    std::array<int, 8> values{8, 5, 2, 1, 34, 2, 2, 5};

    BucketQueue<int> queue;
    EXPECT_TRUE(queue.empty());
    for (int val : values) {
        queue.push(val);
        EXPECT_FALSE(queue.empty());
    }
    EXPECT_EQ(queue.size(), values.size());

    std::sort(values.begin(), values.end());

    for (int val : values) {
        EXPECT_FALSE(queue.empty());
        EXPECT_EQ(queue.top(), val);
        queue.pop();
    };
    EXPECT_TRUE(queue.empty());
}

TEST(BucketQueueTest, Values2) {
    std::array<int, 12> values{9, 23, 4, 1, -5, 123, 23, -23, -82, 0, 0, 1};

    BucketQueue<int> queue;
    EXPECT_TRUE(queue.empty());
    for (int val : values) {
        queue.push(val);
        EXPECT_FALSE(queue.empty());
    }
    EXPECT_EQ(queue.size(), values.size());

    std::sort(values.begin(), values.end());

    for (int val : values) {
        EXPECT_FALSE(queue.empty());
        EXPECT_EQ(queue.top(), val);
        queue.pop();
    };
    EXPECT_TRUE(queue.empty());
}

TEST(BucketQueueTest, Values3) {
    std::array<double, 5> values{0.23, -3.23, 23.934, 93.2902, -0.02};

    BucketQueue<double> queue;
    EXPECT_TRUE(queue.empty());
    for (double val : values) {
        queue.push(val);
        EXPECT_FALSE(queue.empty());
    }
    EXPECT_EQ(queue.size(), values.size());

    std::sort(values.begin(), values.end());

    for (double val : values) {
        EXPECT_FALSE(queue.empty());
        EXPECT_EQ(queue.top(), val);
        queue.pop();
    };
    EXPECT_TRUE(queue.empty());
}

TEST(BucketQueueTest, values4) {
    constexpr std::size_t batch = 20U;
    constexpr std::size_t numIt = 50U;

    std::vector<int> values(numIt * batch);
    std::iota(values.begin(), values.end(), 27);

    BucketQueue<int> queue;
    EXPECT_TRUE(queue.empty());

    std::size_t cntr = 0U;
    for (auto it = values.cbegin(); it != values.cend();) {
        EXPECT_EQ(queue.size(), cntr);

        auto endIt = std::next(it, batch);
        for (auto bIt = it; bIt != endIt; ++bIt) {
            queue.push(*bIt);
        }

        EXPECT_FALSE(queue.empty());
        cntr += batch;
        EXPECT_EQ(queue.size(), cntr);

        while (it != endIt) {
            int result = queue.top();
            EXPECT_EQ(*it, result);
            ++it;

            queue.pop();
            EXPECT_EQ(queue.size(), --cntr);
        }
    }
}
