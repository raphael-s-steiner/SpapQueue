#pragma once

#include "ParallelPriotityQueue/SpapQueueWorker.hpp"

namespace spapq {

/**
 * @brief An spapQueue worker which spawns two tasks when processing a task, much like the (terrible)
 * recursive implementation to compute Fibonacci numbers.
 *
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
class FibonacciWorker final : public WorkerResource<GlobalQType, LocalQType, numPorts> {
    template <typename, typename, std::size_t>
    friend class FibonacciWorker;
    template <typename, QNetwork, template <class, class, std::size_t> class, typename>
    friend class SpapQueue;

    using BaseT = WorkerResource<GlobalQType, LocalQType, numPorts>;
    using value_type = BaseT::value_type;

  protected:
    inline void processElement(const value_type val) noexcept override {
        if (val > 0) { this->enqueueGlobal(val - 1); }
        if (val > 1) { this->enqueueGlobal(val - 2); }
    }

  public:
    template <std::size_t channelIndicesLength>
    constexpr FibonacciWorker(GlobalQType &globalQueue,
                              const std::array<std::size_t, channelIndicesLength> &channelIndices,
                              std::size_t workerId) :
        WorkerResource<GlobalQType, LocalQType, numPorts>(globalQueue, channelIndices, workerId){};

    FibonacciWorker(const FibonacciWorker &other) = delete;
    FibonacciWorker(FibonacciWorker &&other) = delete;
    FibonacciWorker &operator=(const FibonacciWorker &other) = delete;
    FibonacciWorker &operator=(FibonacciWorker &&other) = delete;
    virtual ~FibonacciWorker() = default;
};

}        // end namespace spapq
