#include <gtest/gtest.h>

#include <initializer_list>

#include "parallelPriotityQueue/QNetwork.hpp"
#include "parallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"
#include "parallelPriotityQueue/GraphExamples/LineGraph.hpp"
#include "parallelPriotityQueue/GraphExamples/PetersenGraph.hpp"

using namespace spapq;

TEST(QNetworkTest, Constructors1) {
    constexpr QNetwork<4, 4> netw({0, 1, 2, 3, 4}, {1, 2, 3, 0}, {10, 9, 8, 7}, {1, 2, 3, 4});
    EXPECT_EQ(netw.numWorkers_, 4);
    EXPECT_EQ(netw.numChannels_, 4);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(netw.vertexPointer_[i], i);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(netw.edgeTargets_[i], (i + 1) % 4);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(netw.multiplicities_[i], 10U - i);
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
        EXPECT_EQ(netw.multiplicities_[i], 1);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(netw.batchSize_[i], 1);
    }
}

TEST(QNetworkTest, Ports1) {
    constexpr QNetwork<4, 4> netw({0, 1, 2, 3, 4}, {1, 2, 3, 0});
    std::vector<std::vector<std::size_t>> outGraph(netw.numWorkers_);
    std::vector<std::vector<std::size_t>> inGraph(netw.numWorkers_);

    for (std::size_t src = 0; src < netw.numWorkers_; ++src) {
        for (std::size_t tgtPtr = netw.vertexPointer_[src]; tgtPtr < netw.vertexPointer_[src + 1]; ++tgtPtr) {
            const std::size_t tgt = netw.edgeTargets_[tgtPtr];
            outGraph[src].push_back(tgt);
            inGraph[tgt].push_back(src);
        }
    }

    for (std::size_t worker = 0; worker < netw.numWorkers_; ++worker) {
        EXPECT_EQ(netw.numPorts_[worker], inGraph[worker].size());
    }

    std::vector<std::vector<bool>> ports(netw.numWorkers_);
    for (std::size_t worker = 0; worker < netw.numWorkers_; ++worker) {
        ports[worker] = std::vector<bool>(inGraph[worker].size(), false);
    }

    for (std::size_t src = 0; src < netw.numWorkers_; ++src) {
        for (std::size_t tgtPtr = netw.vertexPointer_[src]; tgtPtr < netw.vertexPointer_[src + 1]; ++tgtPtr) {
            const std::size_t tgt = netw.edgeTargets_[tgtPtr];
            const std::size_t port = netw.targetPort_[tgtPtr];
            EXPECT_GE(port, 0U);
            EXPECT_LT(port, inGraph[tgt].size());
            EXPECT_FALSE(ports[tgt][port]);
            ports[tgt][port] = true;
        }
    }

    for (const auto &vec : ports) {
        for (const bool &val : vec) {
            EXPECT_TRUE(val);
        }
    }

    EXPECT_TRUE(netw.isValidQNetwork());
}

TEST(QNetworkTest, Ports2) {
    constexpr QNetwork<10, 30> netw = PETERSEN_GRAPH;
    std::vector<std::vector<std::size_t>> outGraph(netw.numWorkers_);
    std::vector<std::vector<std::size_t>> inGraph(netw.numWorkers_);

    for (std::size_t src = 0; src < netw.numWorkers_; ++src) {
        for (std::size_t tgtPtr = netw.vertexPointer_[src]; tgtPtr < netw.vertexPointer_[src + 1]; ++tgtPtr) {
            const std::size_t tgt = netw.edgeTargets_[tgtPtr];
            outGraph[src].push_back(tgt);
            inGraph[tgt].push_back(src);
        }
    }

    for (std::size_t worker = 0; worker < netw.numWorkers_; ++worker) {
        EXPECT_EQ(netw.numPorts_[worker], inGraph[worker].size());
    }

    std::vector<std::vector<bool>> ports(netw.numWorkers_);
    for (std::size_t worker = 0; worker < netw.numWorkers_; ++worker) {
        ports[worker] = std::vector<bool>(inGraph[worker].size(), false);
    }

    for (std::size_t src = 0; src < netw.numWorkers_; ++src) {
        for (std::size_t tgtPtr = netw.vertexPointer_[src]; tgtPtr < netw.vertexPointer_[src + 1]; ++tgtPtr) {
            const std::size_t tgt = netw.edgeTargets_[tgtPtr];
            const std::size_t port = netw.targetPort_[tgtPtr];
            EXPECT_GE(port, 0U);
            EXPECT_LT(port, inGraph[tgt].size());
            EXPECT_FALSE(ports[tgt][port]);
            ports[tgt][port] = true;
        }
    }

    for (const auto &vec : ports) {
        for (const bool &val : vec) {
            EXPECT_TRUE(val);
        }
    }

    EXPECT_TRUE(netw.isValidQNetwork());
}

TEST(QNetworkTest, Validity) {
    EXPECT_TRUE(PETERSEN_GRAPH.isValidQNetwork());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<1>().isValidQNetwork());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<2>().isValidQNetwork());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<3>().isValidQNetwork());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<4>().isValidQNetwork());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<7>().isValidQNetwork());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<12>().isValidQNetwork());
}