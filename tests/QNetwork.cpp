#include <gtest/gtest.h>

#include <initializer_list>

#include "ParallelPriotityQueue/QNetwork.hpp"
#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"
#include "ParallelPriotityQueue/GraphExamples/LineGraph.hpp"
#include "ParallelPriotityQueue/GraphExamples/PetersenGraph.hpp"

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
    
    EXPECT_EQ(netw.maxBatchSize(), 4);
    EXPECT_TRUE(netw.hasHomogeneousInPorts());
    EXPECT_TRUE(netw.hasHomogeneousOutPorts());
    EXPECT_TRUE(netw.hasHomogeneousPorts());
    EXPECT_EQ(netw.maxPortNum(), 1U);
    EXPECT_EQ(netw.bufferSize_, 16U);
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
    constexpr QNetwork<8, 64> netw = FULLY_CONNECTED_GRAPH<8>(); 
    EXPECT_TRUE(netw.isValidQNetwork());
}

TEST(QNetworkTest, LineGraphNumEdges) {
    EXPECT_EQ(lineGraphNumEdges(PETERSEN_GRAPH), 90);

    constexpr QNetwork<4, 4> netw({0, 1, 2, 3, 4}, {1, 2, 3, 0});
    EXPECT_EQ(lineGraphNumEdges(netw), 4);

    EXPECT_EQ(lineGraphNumEdges(FULLY_CONNECTED_GRAPH<1>()), 1 * 1 * 1);
    EXPECT_EQ(lineGraphNumEdges(FULLY_CONNECTED_GRAPH<2>()), 2 * 2 * 2);
    EXPECT_EQ(lineGraphNumEdges(FULLY_CONNECTED_GRAPH<3>()), 3 * 3 * 3);
    EXPECT_EQ(lineGraphNumEdges(FULLY_CONNECTED_GRAPH<5>()), 5 * 5 * 5);
    EXPECT_EQ(lineGraphNumEdges(FULLY_CONNECTED_GRAPH<8>()), 8 * 8 * 8);
    EXPECT_EQ(lineGraphNumEdges(FULLY_CONNECTED_GRAPH<13>()), 13 * 13 * 13);
    EXPECT_EQ(lineGraphNumEdges(FULLY_CONNECTED_GRAPH<21>()), 21 * 21 * 21);
}

TEST(QNetworkTest, LineGraph) {
    constexpr auto graph = QNetwork<2, 4>({0, 2, 4}, {0, 1, 1, 0}, {1, 1, 1, 1}, {1, 2, 1, 2});
    EXPECT_TRUE(graph.isValidQNetwork());
    EXPECT_TRUE(LINE_GRAPH(graph).isValidQNetwork());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(graph)).isValidQNetwork());

    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<2>()).isValidQNetwork());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<2>())).isValidQNetwork());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<2>()))).isValidQNetwork());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<2>())))).isValidQNetwork());

    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3>()).isValidQNetwork());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3>())).isValidQNetwork());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3>()))).isValidQNetwork());

    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5>()).isValidQNetwork());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5>())).isValidQNetwork());

    EXPECT_TRUE(LINE_GRAPH(PETERSEN_GRAPH).isValidQNetwork());
}

TEST(QNetworkTest, PortNumbers) {
    EXPECT_TRUE(PETERSEN_GRAPH.hasHomogeneousInPorts());
    EXPECT_TRUE(PETERSEN_GRAPH.hasHomogeneousOutPorts());
    EXPECT_TRUE(PETERSEN_GRAPH.hasHomogeneousPorts());
    EXPECT_EQ(PETERSEN_GRAPH.maxPortNum(), 3U);

    
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<3U>().hasHomogeneousInPorts());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<3U>().hasHomogeneousOutPorts());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<3U>().hasHomogeneousPorts());
    EXPECT_EQ(FULLY_CONNECTED_GRAPH<3U>().maxPortNum(), 3U);
    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>()).hasHomogeneousInPorts());
    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>()).hasHomogeneousOutPorts());
    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>()).hasHomogeneousPorts());
    EXPECT_EQ(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>()).maxPortNum(), 3U);
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>())).hasHomogeneousInPorts());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>())).hasHomogeneousOutPorts());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>())).hasHomogeneousPorts());
    EXPECT_EQ(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>())).maxPortNum(), 3U);

    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<5U>().hasHomogeneousInPorts());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<5U>().hasHomogeneousOutPorts());
    EXPECT_TRUE(FULLY_CONNECTED_GRAPH<5U>().hasHomogeneousPorts());
    EXPECT_EQ(FULLY_CONNECTED_GRAPH<5U>().maxPortNum(), 5U);
    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5U>()).hasHomogeneousInPorts());
    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5U>()).hasHomogeneousOutPorts());
    EXPECT_TRUE(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5U>()).hasHomogeneousPorts());
    EXPECT_EQ(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5U>()).maxPortNum(), 5U);
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5U>())).hasHomogeneousInPorts());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5U>())).hasHomogeneousOutPorts());
    EXPECT_TRUE(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5U>())).hasHomogeneousPorts());
    EXPECT_EQ(LINE_GRAPH(LINE_GRAPH(FULLY_CONNECTED_GRAPH<5U>())).maxPortNum(), 5U);

    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});
    EXPECT_FALSE(netw.hasHomogeneousInPorts());
    EXPECT_FALSE(netw.hasHomogeneousOutPorts());
    EXPECT_FALSE(netw.hasHomogeneousPorts());
    EXPECT_EQ(netw.maxPortNum(), 2U);
    EXPECT_FALSE(LINE_GRAPH(netw).hasHomogeneousPorts());
    EXPECT_EQ(LINE_GRAPH(netw).maxPortNum(), 2U);

    constexpr QNetwork<3, 3> netw2({0, 0, 1, 3}, {1, 0, 2});
    EXPECT_TRUE(netw2.hasHomogeneousInPorts());
    EXPECT_FALSE(netw2.hasHomogeneousOutPorts());
    EXPECT_FALSE(netw2.hasHomogeneousPorts());

    constexpr QNetwork<3, 3> netw3({0, 1, 2, 3}, {0, 0, 2});
    EXPECT_FALSE(netw3.hasHomogeneousInPorts());
    EXPECT_TRUE(netw3.hasHomogeneousOutPorts());
    EXPECT_FALSE(netw3.hasHomogeneousPorts());
}