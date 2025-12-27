#pragma once

#include <atomic>
#include <vector>

#include "ParallelPriotityQueue/SpapQueueWorker.hpp"

namespace spapq {

/**
 * @brief Single Source Shortest Path Worker
 *
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
class SSSPWorker final : public WorkerResource<GlobalQType, LocalQType, numPorts> {
    template <typename, typename, std::size_t>
    friend class SSSPWorker;
    template <typename, QNetwork, template <class, class, std::size_t> class, typename>
    friend class SpapQueue;

    using BaseT = WorkerResource<GlobalQType, LocalQType, numPorts>;
    using value_type = BaseT::value_type;

    const std::vector<unsigned> &sourcePointers_;
    const std::vector<unsigned> &edgeTargets_;

    std::vector<std::atomic<unsigned>> &distance_;

  protected:
    inline void processElement(const value_type val) noexcept override {
        const unsigned dist = val.first;
        const unsigned vertex = val.second;

        unsigned currDist = distance_[vertex].load(std::memory_order_relaxed);
        while (dist < currDist) {
            if (distance_[vertex].compare_exchange_weak(
                    currDist, dist, std::memory_order_relaxed, std::memory_order_relaxed)) {
                const unsigned newDist = dist + 1;
                for (unsigned indx = sourcePointers_[vertex]; indx < sourcePointers_[vertex + 1]; ++indx) {
                    const unsigned tgt = edgeTargets_[indx];
                    if (newDist < distance_[tgt].load(std::memory_order_relaxed)) {
                        this->enqueueGlobal({newDist, tgt});
                    }
                }
            }
        }
    }

  public:
    template <std::size_t channelIndicesLength>
    constexpr SSSPWorker(GlobalQType &globalQueue,
                         const std::array<std::size_t, channelIndicesLength> &channelIndices,
                         std::size_t workerId,
                         const std::vector<unsigned> &sourcePointers,
                         const std::vector<unsigned> &edgeTargets,
                         std::vector<std::atomic<unsigned>> &distance) :
        WorkerResource<GlobalQType, LocalQType, numPorts>(globalQueue, channelIndices, workerId),
        sourcePointers_(sourcePointers),
        edgeTargets_(edgeTargets),
        distance_(distance){};

    SSSPWorker(const SSSPWorker &other) = delete;
    SSSPWorker(SSSPWorker &&other) = delete;
    SSSPWorker &operator=(const SSSPWorker &other) = delete;
    SSSPWorker &operator=(SSSPWorker &&other) = delete;
    virtual ~SSSPWorker() = default;
};

}        // end namespace spapq
