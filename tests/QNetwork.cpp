#include "ParallelPriotityQueue/QNetwork.hpp"

#include <gtest/gtest.h>

#include <initializer_list>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"
#include "ParallelPriotityQueue/GraphExamples/LineGraph.hpp"
#include "ParallelPriotityQueue/GraphExamples/PetersenGraph.hpp"

using namespace spapq;

TEST(QNetworkTest, Constructors1) {
    constexpr QNetwork<4, 4> netw(
        {0, 1, 2, 3, 4}, {1, 2, 3, 0}, {11, 12, 13, 14}, {10, 9, 8, 7}, {1, 2, 3, 4});
    EXPECT_EQ(netw.numWorkers_, 4);
    EXPECT_EQ(netw.numChannels_, 4);
    for (std::size_t i = 0; i < 5; ++i) { EXPECT_EQ(netw.vertexPointer_[i], i); }
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(netw.logicalCore_[i], i + 11U); }
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(netw.edgeTargets_[i], (i + 1) % 4); }
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(netw.multiplicities_[i], 10U - i); }
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(netw.batchSize_[i], (i + 1)); }

    EXPECT_EQ(netw.enqueueFrequency_, 16U);
    EXPECT_EQ(netw.maxBatchSize(), 4U);
    EXPECT_TRUE(netw.hasHomogeneousInPorts());
    EXPECT_TRUE(netw.hasHomogeneousOutPorts());
    EXPECT_TRUE(netw.hasHomogeneousPorts());
    EXPECT_TRUE(netw.hasSeparateLogicalCores());
    EXPECT_EQ(netw.maxPortNum(), 1U);
    EXPECT_EQ(netw.channelBufferSize_, 64U);

    for (std::size_t w = 0U; w < netw.numWorkers_; ++w) { EXPECT_TRUE(netw.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(netw.isStronglyConnected());
}

TEST(QNetworkTest, Constructors2) {
    constexpr QNetwork<4, 4> netw({0, 1, 2, 3, 4}, {1, 2, 3, 0});
    EXPECT_EQ(netw.numWorkers_, 4);
    EXPECT_EQ(netw.numChannels_, 4);
    for (std::size_t i = 0; i < 5; ++i) { EXPECT_EQ(netw.vertexPointer_[i], i); }
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(netw.logicalCore_[i], i); }
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(netw.edgeTargets_[i], (i + 1) % 4); }
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(netw.multiplicities_[i], 1); }
    for (std::size_t i = 0; i < 4; ++i) { EXPECT_EQ(netw.batchSize_[i], 1); }

    EXPECT_TRUE(netw.hasSeparateLogicalCores());
}

TEST(QNetworkTest, Ports1) {
    constexpr QNetwork<4, 4> netw({0, 1, 2, 3, 4}, {1, 2, 3, 0}, {10, 0, 3, 10});
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
        for (const bool &val : vec) { EXPECT_TRUE(val); }
    }

    EXPECT_TRUE(netw.isValidQNetwork());
    EXPECT_FALSE(netw.hasSeparateLogicalCores());
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
        for (const bool &val : vec) { EXPECT_TRUE(val); }
    }

    EXPECT_TRUE(netw.isValidQNetwork());
    EXPECT_TRUE(netw.hasSeparateLogicalCores());
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
    EXPECT_TRUE(netw.hasSeparateLogicalCores());

    for (std::size_t w = 0U; w < netw.numWorkers_; ++w) { EXPECT_TRUE(netw.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(netw.isStronglyConnected());
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
    constexpr auto graph
        = QNetwork<2, 4>({0, 2, 4}, {0, 1, 1, 0}, {0, 1}, {1, 1, 1, 1}, {1, 2, 1, 2}, 17, 33, 6);
    EXPECT_TRUE(graph.isValidQNetwork());
    EXPECT_TRUE(graph.hasSeparateLogicalCores());
    constexpr auto lgraph = LINE_GRAPH(graph);
    constexpr auto llgraph = LINE_GRAPH(lgraph);

    for (std::size_t w = 0U; w < graph.numWorkers_; ++w) { EXPECT_TRUE(graph.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(graph.isStronglyConnected());

    EXPECT_TRUE(lgraph.isValidQNetwork());
    EXPECT_TRUE(lgraph.hasSeparateLogicalCores());
    EXPECT_EQ(lgraph.enqueueFrequency_, graph.enqueueFrequency_);
    EXPECT_EQ(lgraph.channelBufferSize_, graph.channelBufferSize_);
    EXPECT_EQ(lgraph.maxPushAttempts_, graph.maxPushAttempts_);

    for (std::size_t w = 0U; w < lgraph.numWorkers_; ++w) { EXPECT_TRUE(lgraph.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(lgraph.isStronglyConnected());

    EXPECT_TRUE(llgraph.isValidQNetwork());
    EXPECT_TRUE(llgraph.hasSeparateLogicalCores());
    EXPECT_EQ(llgraph.enqueueFrequency_, graph.enqueueFrequency_);
    EXPECT_EQ(llgraph.channelBufferSize_, graph.channelBufferSize_);
    EXPECT_EQ(llgraph.maxPushAttempts_, graph.maxPushAttempts_);

    for (std::size_t w = 0U; w < llgraph.numWorkers_; ++w) { EXPECT_TRUE(llgraph.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(llgraph.isStronglyConnected());

    constexpr auto lfull2 = LINE_GRAPH(FULLY_CONNECTED_GRAPH<2>());
    constexpr auto llfull2 = LINE_GRAPH(lfull2);
    constexpr auto lllfull2 = LINE_GRAPH(llfull2);
    constexpr auto llllfull2 = LINE_GRAPH(lllfull2);
    EXPECT_TRUE(lfull2.isValidQNetwork());
    EXPECT_TRUE(lfull2.hasSeparateLogicalCores());
    EXPECT_TRUE(llfull2.isValidQNetwork());
    EXPECT_TRUE(llfull2.hasSeparateLogicalCores());
    EXPECT_TRUE(lllfull2.isValidQNetwork());
    EXPECT_TRUE(lllfull2.hasSeparateLogicalCores());
    EXPECT_TRUE(llllfull2.isValidQNetwork());
    EXPECT_TRUE(llllfull2.hasSeparateLogicalCores());

    for (std::size_t w = 0U; w < llllfull2.numWorkers_; ++w) {
        EXPECT_TRUE(llllfull2.hasPathToAllWorkers(w));
    }
    EXPECT_TRUE(llllfull2.isStronglyConnected());

    constexpr auto lfull3 = LINE_GRAPH(FULLY_CONNECTED_GRAPH<3>());
    constexpr auto llfull3 = LINE_GRAPH(lfull3);
    constexpr auto lllfull3 = LINE_GRAPH(llfull3);
    EXPECT_TRUE(lfull3.isValidQNetwork());
    EXPECT_TRUE(lfull3.hasSeparateLogicalCores());
    EXPECT_TRUE(llfull3.isValidQNetwork());
    EXPECT_TRUE(llfull3.hasSeparateLogicalCores());
    EXPECT_TRUE(lllfull3.isValidQNetwork());
    EXPECT_TRUE(lllfull3.hasSeparateLogicalCores());

    for (std::size_t w = 0U; w < lllfull3.numWorkers_; ++w) { EXPECT_TRUE(lllfull3.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(lllfull3.isStronglyConnected());

    constexpr auto lfull5 = LINE_GRAPH(FULLY_CONNECTED_GRAPH<5>());
    constexpr auto llfull5 = LINE_GRAPH(lfull5);
    EXPECT_TRUE(lfull5.isValidQNetwork());
    EXPECT_TRUE(lfull5.hasSeparateLogicalCores());
    EXPECT_TRUE(llfull5.isValidQNetwork());
    EXPECT_TRUE(llfull5.hasSeparateLogicalCores());

    for (std::size_t w = 0U; w < llfull5.numWorkers_; ++w) { EXPECT_TRUE(llfull5.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(llfull5.isStronglyConnected());

    constexpr auto lPet = LINE_GRAPH(PETERSEN_GRAPH);
    EXPECT_TRUE(lPet.isValidQNetwork());
    EXPECT_TRUE(lPet.hasSeparateLogicalCores());

    for (std::size_t w = 0U; w < lPet.numWorkers_; ++w) { EXPECT_TRUE(lPet.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(lPet.isStronglyConnected());
}

TEST(QNetworkTest, PortNumbers) {
    EXPECT_TRUE(PETERSEN_GRAPH.hasHomogeneousInPorts());
    EXPECT_TRUE(PETERSEN_GRAPH.hasHomogeneousOutPorts());
    EXPECT_TRUE(PETERSEN_GRAPH.hasHomogeneousPorts());
    EXPECT_EQ(PETERSEN_GRAPH.maxPortNum(), 3U);

    constexpr auto full3 = FULLY_CONNECTED_GRAPH<3U>();
    constexpr auto lfull3 = LINE_GRAPH(full3);
    constexpr auto llfull3 = LINE_GRAPH(lfull3);
    EXPECT_TRUE(full3.hasHomogeneousInPorts());
    EXPECT_TRUE(full3.hasHomogeneousOutPorts());
    EXPECT_TRUE(full3.hasHomogeneousPorts());
    EXPECT_EQ(full3.maxPortNum(), 3U);
    EXPECT_TRUE(lfull3.hasHomogeneousInPorts());
    EXPECT_TRUE(lfull3.hasHomogeneousOutPorts());
    EXPECT_TRUE(lfull3.hasHomogeneousPorts());
    EXPECT_EQ(lfull3.maxPortNum(), 3U);
    EXPECT_TRUE(llfull3.hasHomogeneousInPorts());
    EXPECT_TRUE(llfull3.hasHomogeneousOutPorts());
    EXPECT_TRUE(llfull3.hasHomogeneousPorts());
    EXPECT_EQ(llfull3.maxPortNum(), 3U);

    constexpr auto full5 = FULLY_CONNECTED_GRAPH<5U>();
    constexpr auto lfull5 = LINE_GRAPH(full5);
    constexpr auto llfull5 = LINE_GRAPH(lfull5);
    EXPECT_TRUE(full5.hasHomogeneousInPorts());
    EXPECT_TRUE(full5.hasHomogeneousOutPorts());
    EXPECT_TRUE(full5.hasHomogeneousPorts());
    EXPECT_EQ(full5.maxPortNum(), 5U);
    EXPECT_TRUE(lfull5.hasHomogeneousInPorts());
    EXPECT_TRUE(lfull5.hasHomogeneousOutPorts());
    EXPECT_TRUE(lfull5.hasHomogeneousPorts());
    EXPECT_EQ(lfull5.maxPortNum(), 5U);
    EXPECT_TRUE(llfull5.hasHomogeneousInPorts());
    EXPECT_TRUE(llfull5.hasHomogeneousOutPorts());
    EXPECT_TRUE(llfull5.hasHomogeneousPorts());
    EXPECT_EQ(llfull5.maxPortNum(), 5U);

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

TEST(QNetworkTest, SelfPush) {
    constexpr QNetwork<1, 1> netw1 = FULLY_CONNECTED_GRAPH<1>();
    for (std::size_t i = 0U; i < netw1.numWorkers_; ++i) {
        EXPECT_EQ(netw1.edgeTargets_[netw1.vertexPointer_[i]], netw1.numWorkers_);
        for (std::size_t edge = netw1.vertexPointer_[i] + 1U; edge < netw1.vertexPointer_[i + 1U]; ++edge) {
            EXPECT_FALSE(netw1.edgeTargets_[edge] == netw1.numWorkers_);
        }
    }

    constexpr QNetwork<2, 4> netw2 = FULLY_CONNECTED_GRAPH<2>();
    for (std::size_t i = 0U; i < netw2.numWorkers_; ++i) {
        EXPECT_EQ(netw2.edgeTargets_[netw2.vertexPointer_[i]], netw2.numWorkers_);
        for (std::size_t edge = netw2.vertexPointer_[i] + 1U; edge < netw2.vertexPointer_[i + 1U]; ++edge) {
            EXPECT_FALSE(netw2.edgeTargets_[edge] == netw2.numWorkers_);
        }
    }

    constexpr QNetwork<4, 16> netw4 = FULLY_CONNECTED_GRAPH<4>();
    for (std::size_t i = 0U; i < netw4.numWorkers_; ++i) {
        EXPECT_EQ(netw4.edgeTargets_[netw4.vertexPointer_[i]], netw4.numWorkers_);
        for (std::size_t edge = netw4.vertexPointer_[i] + 1U; edge < netw4.vertexPointer_[i + 1U]; ++edge) {
            EXPECT_FALSE(netw4.edgeTargets_[edge] == netw4.numWorkers_);
        }
    }

    constexpr QNetwork<7, 49> netw7 = FULLY_CONNECTED_GRAPH<7>();
    for (std::size_t i = 0U; i < netw7.numWorkers_; ++i) {
        EXPECT_EQ(netw7.edgeTargets_[netw7.vertexPointer_[i]], netw7.numWorkers_);
        for (std::size_t edge = netw7.vertexPointer_[i] + 1U; edge < netw7.vertexPointer_[i + 1U]; ++edge) {
            EXPECT_FALSE(netw7.edgeTargets_[edge] == netw7.numWorkers_);
        }
    }

    constexpr QNetwork<3, 6> netw({0, 3, 5, 6}, {0, 3, 1, 3, 2, 0});
    EXPECT_EQ(netw.numPorts_[0], 3);
    EXPECT_EQ(netw.numPorts_[1], 2);
    EXPECT_EQ(netw.numPorts_[2], 1);
}

TEST(QNetworkTest, Connectivity) {
    constexpr QNetwork<3, 6> netw1({0, 3, 5, 6}, {0, 3, 1, 3, 2, 0});
    for (std::size_t w = 0U; w < netw1.numWorkers_; ++w) { EXPECT_TRUE(netw1.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(netw1.isStronglyConnected());

    constexpr QNetwork<2, 3> netw2({0, 1, 3}, {1, 0, 1});
    for (std::size_t w = 0U; w < netw2.numWorkers_; ++w) { EXPECT_TRUE(netw2.hasPathToAllWorkers(w)); }
    EXPECT_TRUE(netw2.isStronglyConnected());

    constexpr QNetwork<3, 4> netw3({0, 0, 1, 4}, {1, 0, 3, 1});
    EXPECT_FALSE(netw3.hasPathToAllWorkers(0));
    EXPECT_FALSE(netw3.hasPathToAllWorkers(1));
    EXPECT_TRUE(netw3.hasPathToAllWorkers(2));
    EXPECT_FALSE(netw3.isStronglyConnected());

    constexpr QNetwork<3, 3> netw4({0, 1, 2, 3}, {0, 0, 2});
    EXPECT_FALSE(netw4.hasPathToAllWorkers(0));
    EXPECT_FALSE(netw4.hasPathToAllWorkers(1));
    EXPECT_FALSE(netw4.hasPathToAllWorkers(2));
    EXPECT_FALSE(netw4.isStronglyConnected());
}

TEST(QNetworkTest, PrintQNetwork) {
    constexpr QNetwork<10, 30> netw = PETERSEN_GRAPH;
    netw.printQNetwork();
}
