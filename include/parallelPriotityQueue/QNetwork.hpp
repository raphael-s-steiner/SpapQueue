#pragma once

#include <array>

namespace spapq {

template<std::size_t workers, std::size_t channels>
struct QNetwork {
    std::size_t numWorkers_;
    std::size_t numChannels_;
    // Graph CSR
    std::array<std::size_t, workers + 1> vertexPointer_;
    std::array<std::size_t, workers> numPorts_;
    std::array<std::size_t, channels> edgeTargets_;
    std::array<std::size_t, channels> targetPort_;
    std::array<std::size_t, channels> batchSize_;

    constexpr void setDefaultBatchSize();
    constexpr void assignTargetPorts();

    constexpr QNetwork(std::array<std::size_t, workers + 1> vertexPointer, std::array<std::size_t, channels> edgeTargets, std::array<std::size_t, channels> batchSize) : numWorkers_(workers), numChannels_(channels), vertexPointer_(vertexPointer), edgeTargets_(edgeTargets), batchSize_(batchSize) { assignTargetPorts(); };
    constexpr QNetwork(std::array<std::size_t, workers + 1> vertexPointer, std::array<std::size_t, channels> edgeTargets) : numWorkers_(workers), numChannels_(channels), vertexPointer_(vertexPointer), edgeTargets_(edgeTargets) { setDefaultBatchSize(); assignTargetPorts(); };
};

template<std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultBatchSize() {
    for (std::size_t i = 0U; i < batchSize_.size(); ++i) {
        batchSize_[i] = 1U;
    }
}

template<std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::assignTargetPorts() {
    for (std::size_t i = 0U; i < numPorts_.size(); ++i) {
        numPorts_[i] = 0U;
    }
    for (std::size_t edge = 0U; edge < edgeTargets_.size(); ++edge) {
        targetPort_[edge] = numPorts_[edgeTargets_[edge]]++;
    }
}

} // end namespace spapq