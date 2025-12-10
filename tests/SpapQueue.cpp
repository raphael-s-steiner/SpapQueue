#include "ParallelPriotityQueue/SpapQueue.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"

using namespace spapq;
using DivisorLocalQueueType
    = std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>>;

constexpr std::size_t divisorTestMaxSize = 100000;

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
class DivisorWorker : public WorkerResource<GlobalQType, LocalQType, numPorts> {
    template <typename, typename, std::size_t>
    friend class DivisorWorker;
    template <typename, QNetwork, template <class, class, std::size_t> class, typename>
    friend class SpapQueue;

    using value_type = WorkerResource<GlobalQType, DivisorLocalQueueType, numPorts>::value_type;

  protected:
    inline void processElement(const value_type &val) override {
        for (value_type i = 2 * val; i < divisorTestMaxSize; i += val) { this->enqueueGlobal(i); }
    }

  public:
    template <std::size_t channelIndicesLength>
    constexpr DivisorWorker(GlobalQType &globalQueue,
                            const std::array<std::size_t, channelIndicesLength> &channelIndices) :
        WorkerResource<GlobalQType, LocalQType, numPorts>(globalQueue, channelIndices){};

    template <std::size_t workerId>
    static constexpr DivisorWorker<GlobalQType, LocalQType, numPorts> QNetworkWorkerResource(
        GlobalQType &globalQueue) {
        return DivisorWorker(globalQueue, tables::qNetworkTable<GlobalQType::netw_, workerId>());
    }

    DivisorWorker(const DivisorWorker &other) = delete;
    DivisorWorker(DivisorWorker &&other) = delete;
    DivisorWorker &operator=(const DivisorWorker &other) = delete;
    DivisorWorker &operator=(DivisorWorker &&other) = delete;
    virtual ~DivisorWorker() = default;
};

std::vector<std::size_t> computeAnswerDivisors(std::size_t N) {
    std::vector<std::size_t> count(N + 1, 1U);
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
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
}

TEST(SpapQueueTest, EmptyQueue1) {
    constexpr QNetwork<1, 1> netw = FULLY_CONNECTED_GRAPH<1U>();

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    globalQ.initQueue();
    globalQ.processQueue();
    globalQ.waitProcessFinish();
}

TEST(SpapQueueTest, EmptyQueue2) {
    constexpr QNetwork<2, 3> netw({0, 1, 3}, {1, 0, 1});

    SpapQueue<std::size_t, netw, DivisorWorker, DivisorLocalQueueType> globalQ;
    globalQ.initQueue();
    globalQ.processQueue();
    globalQ.waitProcessFinish();
}