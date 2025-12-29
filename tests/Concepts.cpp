/*
Copyright 2025 Raphael S. Steiner

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

#include <gtest/gtest.h>

#include <queue>
#include <utility>
#include <vector>

#include "ParallelPriotityQueue/Concepts/BasicQueue.hpp"

using namespace spapq;

TEST(ConceptsTest, BasicQueueTest) {
    EXPECT_TRUE(BasicQueue<std::priority_queue<unsigned>>);

    using IntLongPair = std::pair<int, long>;
    EXPECT_TRUE(BasicQueue<std::priority_queue<IntLongPair>>);
    
    EXPECT_FALSE(BasicQueue<IntLongPair>);

    EXPECT_FALSE(BasicQueue<std::vector<std::size_t>>);

    EXPECT_FALSE(BasicQueue<std::vector<long unsigned>::iterator>);
}
