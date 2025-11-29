#include "ParallelPriotityQueue/SpapQueue.hpp"

#include <gtest/gtest.h>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"

using namespace spapq;

TEST(SpapQueueTest, Constructors) {
    constexpr QNetwork<1, 2> netw({0, 2}, {0, 0});
    EXPECT_TRUE(netw.hasHomogeneousInPorts());
    EXPECT_TRUE(netw.hasHomogeneousOutPorts());
    EXPECT_TRUE(netw.hasHomogeneousPorts());
    EXPECT_EQ(netw.maxPortNum(), 2U);
}
