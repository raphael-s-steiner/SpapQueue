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

#include "ParallelPriotityQueue/SpapQueue.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"
#include "ParallelPriotityQueue/WorkerExamples/SSSPWorker.hpp"

using namespace spapq;

using DivisorLocalQueueType
    = std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>>;

constexpr std::size_t divisorTestMaxSize = 2000;

template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
class DivisorWorker final : public WorkerResource<GlobalQType, LocalQType, numPorts> {
    template <typename, BasicQueue, std::size_t>
    friend class DivisorWorker;
    template <typename, QNetwork, template <class, BasicQueue, std::size_t> class, BasicQueue>
    friend class SpapQueue;

    using BaseT = WorkerResource<GlobalQType, LocalQType, numPorts>;
    using value_type = BaseT::value_type;

  private:
    std::vector<std::size_t> &locAnsCounter_;

  protected:
    inline void processElement(const value_type val) noexcept override {
        ++locAnsCounter_[val];
        for (value_type i = 2 * val; i < divisorTestMaxSize; i += val) { this->enqueueGlobal(i); }
    }

  public:
    template <std::size_t channelIndicesLength>
    constexpr DivisorWorker(GlobalQType &globalQueue,
                            const std::array<std::size_t, channelIndicesLength> &channelIndices,
                            std::size_t workerId,
                            std::vector<std::vector<std::size_t>> &ansCounter) :
        WorkerResource<GlobalQType, LocalQType, numPorts>(globalQueue, channelIndices, workerId),
        locAnsCounter_(ansCounter[workerId]){}

    DivisorWorker(const DivisorWorker &other) = delete;
    DivisorWorker(DivisorWorker &&other) = delete;
    DivisorWorker &operator=(const DivisorWorker &other) = delete;
    DivisorWorker &operator=(DivisorWorker &&other) = delete;
    virtual ~DivisorWorker() = default;
};

std::vector<std::size_t> computeAnswerDivisors(std::size_t N) {
    std::vector<std::size_t> count(N, 1U);
    count[0U] = 0U;

    for (std::size_t i = 2U; i < count.size(); ++i) {
        for (std::size_t j = 2U; j * j <= i; ++j) {
            if (i % j == 0) {
                count[i] += count[j];
                if (j * j != i) { count[i] += count[i / j]; }
            }
        }
    }

    return count;
}

using FibonacchiLocalQueueType
    = std::priority_queue<std::size_t, std::vector<std::size_t>, std::less<std::size_t>>;

constexpr std::size_t fibonacciTestSize = 26;

template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
class FibonacciWorker final : public WorkerResource<GlobalQType, LocalQType, numPorts> {
    template <typename, BasicQueue, std::size_t>
    friend class FibonacciWorker;
    template <typename, QNetwork, template <class, BasicQueue, std::size_t> class, BasicQueue>
    friend class SpapQueue;

    using BaseT = WorkerResource<GlobalQType, LocalQType, numPorts>;
    using value_type = BaseT::value_type;

  private:
    std::vector<std::size_t> &locAnsCounter_;

  protected:
    inline void processElement(const value_type val) noexcept override {
        ++locAnsCounter_[val];

        if (val > 0) { this->enqueueGlobal(val - 1); }
        if (val > 1) { this->enqueueGlobal(val - 2); }
    }

  public:
    template <std::size_t channelIndicesLength>
    constexpr FibonacciWorker(GlobalQType &globalQueue,
                              const std::array<std::size_t, channelIndicesLength> &channelIndices,
                              std::size_t workerId,
                              std::vector<std::vector<std::size_t>> &ansCounter) :
        WorkerResource<GlobalQType, LocalQType, numPorts>(globalQueue, channelIndices, workerId),
        locAnsCounter_(ansCounter[workerId]){}

    FibonacciWorker(const FibonacciWorker &other) = delete;
    FibonacciWorker(FibonacciWorker &&other) = delete;
    FibonacciWorker &operator=(const FibonacciWorker &other) = delete;
    FibonacciWorker &operator=(FibonacciWorker &&other) = delete;
    virtual ~FibonacciWorker() = default;
};

std::vector<std::size_t> computeAnswerFibonacci(std::size_t N) {
    std::vector<std::size_t> count(N, 1U);

    for (std::size_t i = count.size() - 3U; i < count.size(); --i) { count[i] = count[i + 1] + count[i + 2]; }

    return count;
}

constexpr unsigned SSSPTorusSideLength = 80U;

CSRGraph make3DTorus(const unsigned sideLength) {
    CSRGraph graph;

    const unsigned sideLengthSqr = sideLength * sideLength;
    const unsigned numVert = sideLength * sideLength * sideLength;
    graph.sourcePointers_.reserve(numVert);
    graph.edgeTargets_.reserve(6 * numVert);

    for (unsigned i = 0U; i < sideLength; ++i) {
        for (unsigned j = 0U; j < sideLength; ++j) {
            for (unsigned k = 0U; k < sideLength; ++k) {
                graph.sourcePointers_.emplace_back(graph.edgeTargets_.size());

                graph.edgeTargets_.emplace_back(
                    ((k + 1) % sideLength) + (j * sideLength) + (i * sideLengthSqr));
                graph.edgeTargets_.emplace_back(
                    ((k + sideLength - 1) % sideLength) + (j * sideLength) + (i * sideLengthSqr));
                graph.edgeTargets_.emplace_back(
                    k + (((j + 1) % sideLength) * sideLength) + (i * sideLengthSqr));
                graph.edgeTargets_.emplace_back(
                    k + (((j + sideLength - 1) % sideLength) * sideLength) + (i * sideLengthSqr));
                graph.edgeTargets_.emplace_back(
                    k + (j * sideLength) + (((i + 1) % sideLength) * sideLengthSqr));
                graph.edgeTargets_.emplace_back(
                    k + (j * sideLength) + (((i + sideLength - 1) % sideLength) * sideLengthSqr));
            }
        }
    }
    graph.sourcePointers_.emplace_back(graph.edgeTargets_.size());

    return graph;
}

TEST(SpapQueueTest, Constructors1) {
    constexpr QNetwork<1, 1> netw = FULLY_CONNECTED_GRAPH<1U>();

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
}

TEST(SpapQueueTest, Constructors2) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
}

TEST(SpapQueueTest, Constructors3) {
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
}

TEST(SpapQueueTest, EmptyQueue1) {
    constexpr QNetwork<1, 1> netw = FULLY_CONNECTED_GRAPH<1U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    EXPECT_FALSE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.processQueue();
    globalQ.waitProcessFinish();
}

TEST(SpapQueueTest, EmptyQueue2) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    EXPECT_FALSE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.processQueue();
    globalQ.waitProcessFinish();
}

TEST(SpapQueueTest, EmptyQueue3) {
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    EXPECT_FALSE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.processQueue();
    globalQ.waitProcessFinish();
}

TEST(SpapQueueTest, Destructor1) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
}

TEST(SpapQueueTest, Destructor2) {
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
}

TEST(SpapQueueTest, DivisorsSingleWorker) {
    constexpr QNetwork<1, 1> netw = FULLY_CONNECTED_GRAPH<1U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerDivisors(divisorTestMaxSize);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}

TEST(SpapQueueTest, DivisorsHomogeneousWorkers) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerDivisors(divisorTestMaxSize);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}

TEST(SpapQueueTest, DivisorsHeterogeneousWorkers) {
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerDivisors(divisorTestMaxSize);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}

TEST(SpapQueueTest, DivisorsPushSafeHomogeneousWorkers) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.processQueue();

    std::size_t count = 1U;

    if (globalQ.pushDuringProcessing<0>(1U)) { ++count; }
    if (globalQ.pushDuringProcessing<4>(1U)) { ++count; }
    if (globalQ.pushDuringProcessing<8>(1U)) { ++count; }
    if (globalQ.pushDuringProcessing<12>(1U)) { ++count; }

    if constexpr (divisorTestMaxSize >= 5000) { EXPECT_EQ(count, 5U); }

    globalQ.waitProcessFinish();

    EXPECT_FALSE(globalQ.pushDuringProcessing<0>(1U));
    EXPECT_FALSE(globalQ.pushDuringProcessing<4>(1U));
    EXPECT_FALSE(globalQ.pushDuringProcessing<8>(1U));
    EXPECT_FALSE(globalQ.pushDuringProcessing<12>(1U));

    std::vector<std::size_t> solution = computeAnswerDivisors(divisorTestMaxSize);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i] * count); }
}

TEST(SpapQueueTest, DivisorsPushSafeHeterogeneousWorkers) {
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.processQueue();

    std::size_t count = 1U;

    if (globalQ.pushDuringProcessing<2>(1U)) { ++count; }

    if constexpr (divisorTestMaxSize >= 5000) { EXPECT_EQ(count, 2U); }

    globalQ.waitProcessFinish();

    EXPECT_FALSE(globalQ.pushDuringProcessing<2>(1U));

    std::vector<std::size_t> solution = computeAnswerDivisors(divisorTestMaxSize);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i] * count); }
}

TEST(SpapQueueTest, ReuseQueue) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerDivisors(divisorTestMaxSize);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }

    // Clearing counts
    for (auto &vec : ansCounter) {
        for (auto &val : vec) { val = 0; }
    }

    // Restarting Queue
    std::cout << "Restarting\n";
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}

TEST(SpapQueueTest, ReuseQueue2) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(divisorTestMaxSize, 0));

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.requestStop();
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerDivisors(divisorTestMaxSize);

    // Clearing counts
    for (auto &vec : ansCounter) {
        for (auto &val : vec) { val = 0; }
    }

    // Restarting Queue
    std::cout << "Restarting\n";
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(1U, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}

TEST(SpapQueueTest, FibonacciSingleWorker) {
    constexpr QNetwork<1, 1> netw = FULLY_CONNECTED_GRAPH<1U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(fibonacciTestSize + 1, 0));

    SpapQueue<std::size_t, netw, FibonacciWorker, FibonacchiLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(fibonacciTestSize, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerFibonacci(fibonacciTestSize + 1);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < fibonacciTestSize + 1; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < fibonacciTestSize + 1; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}

TEST(SpapQueueTest, FibonacciHomogeneousWorkers) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(fibonacciTestSize + 1, 0));

    SpapQueue<std::size_t, netw, FibonacciWorker, FibonacchiLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(fibonacciTestSize, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerFibonacci(fibonacciTestSize + 1);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < fibonacciTestSize + 1; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < fibonacciTestSize + 1; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}

TEST(SpapQueueTest, FibonacciHeterogeneousWorkers) {
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    std::vector<std::vector<std::size_t>> ansCounter(netw.numWorkers_,
                                                     std::vector<std::size_t>(fibonacciTestSize + 1, 0));

    SpapQueue<std::size_t, netw, FibonacciWorker, FibonacchiLocalQueueType> globalQ;
    EXPECT_TRUE(globalQ.initQueue(std::ref(ansCounter)));
    globalQ.pushBeforeProcessing(fibonacciTestSize, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerFibonacci(fibonacciTestSize + 1);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < fibonacciTestSize + 1; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < fibonacciTestSize + 1; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}

TEST(SpapQueueTest, SSSPSingleWorker) {
    constexpr QNetwork<1, 1> netw = FULLY_CONNECTED_GRAPH<1U>();

    SpapQueue<std::array<unsigned, 2U>,
              netw,
              SSSPWorker,
              std::priority_queue<std::array<unsigned, 2U>,
                                  std::vector<std::array<unsigned, 2U>>,
                                  std::greater<std::array<unsigned, 2U>>>>
        globalQ;

    const CSRGraph graph = make3DTorus(SSSPTorusSideLength);
    const unsigned nVerts = SSSPTorusSideLength * SSSPTorusSideLength * SSSPTorusSideLength;

    std::vector<std::atomic<unsigned>> distances(nVerts);
    for (auto &dist : distances) {
        dist.store(std::numeric_limits<unsigned>::max(), std::memory_order_relaxed);
    }
    distances[0].store(0U, std::memory_order_relaxed);

    EXPECT_TRUE(globalQ.initQueue(std::cref(graph), std::ref(distances)));
    globalQ.pushBeforeProcessing({0U, 0U}, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    const unsigned sideLengthSqr = SSSPTorusSideLength * SSSPTorusSideLength;

    for (unsigned i = 0U; i < SSSPTorusSideLength; ++i) {
        for (unsigned j = 0U; j < SSSPTorusSideLength; ++j) {
            for (unsigned k = 0U; k < SSSPTorusSideLength; ++k) {
                const unsigned vert = k + (j * SSSPTorusSideLength) + (i * sideLengthSqr);

                const unsigned dist = std::min(k, SSSPTorusSideLength - k)
                                      + std::min(j, SSSPTorusSideLength - j)
                                      + std::min(i, SSSPTorusSideLength - i);

                EXPECT_EQ(distances[vert].load(std::memory_order_relaxed), dist);
            }
        }
    }
}

TEST(SpapQueueTest, SSSPHomogeneousWorkers) {
    constexpr QNetwork<4, 16> netw = FULLY_CONNECTED_GRAPH<4U>();

    SpapQueue<std::array<unsigned, 2U>,
              netw,
              SSSPWorker,
              std::priority_queue<std::array<unsigned, 2U>,
                                  std::vector<std::array<unsigned, 2U>>,
                                  std::greater<std::array<unsigned, 2U>>>>
        globalQ;

    const CSRGraph graph = make3DTorus(SSSPTorusSideLength);
    const unsigned nVerts = SSSPTorusSideLength * SSSPTorusSideLength * SSSPTorusSideLength;

    std::vector<std::atomic<unsigned>> distances(nVerts);
    for (auto &dist : distances) {
        dist.store(std::numeric_limits<unsigned>::max(), std::memory_order_relaxed);
    }
    distances[0].store(0U, std::memory_order_relaxed);

    EXPECT_TRUE(globalQ.initQueue(std::cref(graph), std::ref(distances)));
    globalQ.pushBeforeProcessing({0U, 0U}, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    const unsigned sideLengthSqr = SSSPTorusSideLength * SSSPTorusSideLength;

    for (unsigned i = 0U; i < SSSPTorusSideLength; ++i) {
        for (unsigned j = 0U; j < SSSPTorusSideLength; ++j) {
            for (unsigned k = 0U; k < SSSPTorusSideLength; ++k) {
                const unsigned vert = k + (j * SSSPTorusSideLength) + (i * sideLengthSqr);

                const unsigned dist = std::min(k, SSSPTorusSideLength - k)
                                      + std::min(j, SSSPTorusSideLength - j)
                                      + std::min(i, SSSPTorusSideLength - i);

                EXPECT_EQ(distances[vert].load(std::memory_order_relaxed), dist);
            }
        }
    }
}

TEST(SpapQueueTest, SSSPHeterogeneousWorkers) {
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    SpapQueue<std::array<unsigned, 2U>,
              netw,
              SSSPWorker,
              std::priority_queue<std::array<unsigned, 2U>,
                                  std::vector<std::array<unsigned, 2U>>,
                                  std::greater<std::array<unsigned, 2U>>>>
        globalQ;

    const CSRGraph graph = make3DTorus(SSSPTorusSideLength);
    const unsigned nVerts = SSSPTorusSideLength * SSSPTorusSideLength * SSSPTorusSideLength;

    std::vector<std::atomic<unsigned>> distances(nVerts);
    for (auto &dist : distances) {
        dist.store(std::numeric_limits<unsigned>::max(), std::memory_order_relaxed);
    }
    distances[0].store(0U, std::memory_order_relaxed);

    EXPECT_TRUE(globalQ.initQueue(std::cref(graph), std::ref(distances)));
    globalQ.pushBeforeProcessing({0U, 0U}, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    const unsigned sideLengthSqr = SSSPTorusSideLength * SSSPTorusSideLength;

    for (unsigned i = 0U; i < SSSPTorusSideLength; ++i) {
        for (unsigned j = 0U; j < SSSPTorusSideLength; ++j) {
            for (unsigned k = 0U; k < SSSPTorusSideLength; ++k) {
                const unsigned vert = k + (j * SSSPTorusSideLength) + (i * sideLengthSqr);

                const unsigned dist = std::min(k, SSSPTorusSideLength - k)
                                      + std::min(j, SSSPTorusSideLength - j)
                                      + std::min(i, SSSPTorusSideLength - i);

                EXPECT_EQ(distances[vert].load(std::memory_order_relaxed), dist);
            }
        }
    }
}
