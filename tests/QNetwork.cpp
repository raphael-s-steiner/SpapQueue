#include <gtest/gtest.h>

#include <initializer_list>

#include "parallelPriotityQueue/QNetwork.hpp"

using namespace spapq;

TEST(QNetworkTest, Constructors1) {
    constexpr QNetwork<4, 4> netw({0, 1, 2, 3, 4}, {1, 2, 3, 0}, {1, 2, 3, 4});
    EXPECT_EQ(netw.numWorkers_, 4);
    EXPECT_EQ(netw.numChannels_, 4);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(netw.vertexPointer_[i], i);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(netw.edgeTargets_[i], (i + 1) % 4);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(netw.batchSize_[i], (i + 1));
    }
}


TEST(QNetworkTest, Constructors2) {
    constexpr QNetwork<4, 4> netw({0, 1, 2, 3, 4}, {1, 2, 3, 0});
    EXPECT_EQ(netw.numWorkers_, 4);
    EXPECT_EQ(netw.numChannels_, 4);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(netw.vertexPointer_[i], i);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(netw.edgeTargets_[i], (i + 1) % 4);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(netw.batchSize_[i], 1);
    }
}