#pragma once

#include <atomic>
#include <vector>

#include "ParallelPriotityQueue/SpapQueueWorker.hpp"

namespace spapq {

/**
 * @brief Compact sparse row graph
 *
 */
struct CSRGraph {
    std::vector<unsigned> sourcePointers_;
    std::vector<unsigned> edgeTargets_;
};

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
    using distance_type = value_type::first_type;
    using vertex_type = value_type::second_type;

    const CSRGraph &graph_;

    std::vector<std::atomic<distance_type>> &distance_;

  protected:
    inline void processElement(const value_type val) noexcept override {
        const distance_type dist = val.first;
        const vertex_type vertex = val.second;

        distance_type currDist = distance_[vertex].load(std::memory_order_relaxed);
        while (dist < currDist) {
            if (distance_[vertex].compare_exchange_weak(
                    currDist, dist, std::memory_order_relaxed, std::memory_order_relaxed)) {
                const distance_type newDist = dist + 1;
                for (vertex_type indx = graph_.sourcePointers_[vertex];
                     indx < graph_.sourcePointers_[vertex + 1];
                     ++indx) {
                    const vertex_type tgt = graph_.edgeTargets_[indx];
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
                         const CSRGraph &graph,
                         std::vector<std::atomic<distance_type>> &distance) :
        WorkerResource<GlobalQType, LocalQType, numPorts>(globalQueue, channelIndices, workerId),
        graph_(graph),
        distance_(distance){};

    SSSPWorker(const SSSPWorker &other) = delete;
    SSSPWorker(SSSPWorker &&other) = delete;
    SSSPWorker &operator=(const SSSPWorker &other) = delete;
    SSSPWorker &operator=(SSSPWorker &&other) = delete;
    virtual ~SSSPWorker() = default;
};

}        // end namespace spapq
