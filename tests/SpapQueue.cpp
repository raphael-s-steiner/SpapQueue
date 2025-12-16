#include "ParallelPriotityQueue/SpapQueue.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"

using namespace spapq;
using DivisorLocalQueueType
    = std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>>;

static constexpr std::size_t divisorTestMaxSize = 10000;

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
class DivisorWorker final : public WorkerResource<GlobalQType, LocalQType, numPorts> {
    template <typename, typename, std::size_t>
    friend class DivisorWorker;
    template <typename, QNetwork, template <class, class, std::size_t> class, typename>
    friend class SpapQueue;

    using BaseT = WorkerResource<GlobalQType, DivisorLocalQueueType, numPorts>;
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
        locAnsCounter_(ansCounter[workerId]){};

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
    globalQ.pushUnsafe(1U, 0U);
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
    globalQ.pushUnsafe(1U, 0U);
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
    globalQ.pushUnsafe(1U, 0U);
    globalQ.processQueue();
    globalQ.waitProcessFinish();

    std::vector<std::size_t> solution = computeAnswerDivisors(divisorTestMaxSize);

    // Tallying up from all workers
    for (std::size_t i = 1; i < netw.numWorkers_; ++i) {
        for (std::size_t j = 0; j < divisorTestMaxSize; ++j) { ansCounter[0][j] += ansCounter[i][j]; }
    }

    for (std::size_t i = 0; i < divisorTestMaxSize; ++i) { EXPECT_EQ(ansCounter[0][i], solution[i]); }
}