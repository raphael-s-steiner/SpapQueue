#pragma once

#include <algorithm>
#include <array>
#include <iostream>

namespace spapq {

template<std::size_t workers, std::size_t channels>
struct QNetwork {
    const std::size_t numWorkers_;
    const std::size_t numChannels_;
    // Graph CSR
    std::array<std::size_t, workers + 1> vertexPointer_;
    std::array<std::size_t, workers> numPorts_;
    std::array<std::size_t, channels> edgeTargets_;
    std::array<std::size_t, channels> multiplicities_;
    std::array<std::size_t, channels> targetPort_;
    std::array<std::size_t, channels> batchSize_;

    constexpr void setDefaultMultiplicities();
    constexpr void setDefaultBatchSize();
    constexpr void assignTargetPorts();

    constexpr bool isValidQNetwork() const;

    void printQNetwork() const;

    constexpr QNetwork(std::array<std::size_t, workers + 1> vertexPointer, std::array<std::size_t, channels> edgeTargets, std::array<std::size_t, channels> multiplicities, std::array<std::size_t, channels> batchSize) : numWorkers_(workers), numChannels_(channels), vertexPointer_(vertexPointer), edgeTargets_(edgeTargets), multiplicities_(multiplicities), batchSize_(batchSize) { assignTargetPorts(); };
    constexpr QNetwork(std::array<std::size_t, workers + 1> vertexPointer, std::array<std::size_t, channels> edgeTargets, std::array<std::size_t, channels> multiplicities) : numWorkers_(workers), numChannels_(channels), vertexPointer_(vertexPointer), edgeTargets_(edgeTargets), multiplicities_(multiplicities) { setDefaultBatchSize(); assignTargetPorts(); };
    constexpr QNetwork(std::array<std::size_t, workers + 1> vertexPointer, std::array<std::size_t, channels> edgeTargets) : numWorkers_(workers), numChannels_(channels), vertexPointer_(vertexPointer), edgeTargets_(edgeTargets) { setDefaultMultiplicities(); setDefaultBatchSize(); assignTargetPorts(); };
};

template<std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultMultiplicities() {
    for (std::size_t i = 0U; i < multiplicities_.size(); ++i) {
        multiplicities_[i] = 1U;
    }
}

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

template<std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::isValidQNetwork() const {
    if ( not std::all_of(multiplicities_.cbegin(), multiplicities_.cend(), [](const std::size_t& mul) { return mul > 0U;}) ) {
        return false;
    }
    if ( not std::all_of(batchSize_.cbegin(), batchSize_.cend(), [](const std::size_t& batch) { return batch > 0U;}) ) {
        return false;
    }
    if ( not std::all_of(numPorts_.cbegin(), numPorts_.cend(), [](const std::size_t& ports) { return ports > 0U;}) ) {
        return false;
    }

    for (std::size_t worker = 0U; worker < numWorkers_; ++worker) {
        if (vertexPointer_[worker] == vertexPointer_[worker + 1]) {
            return false;
        }
    }

    return true;
}

template<std::size_t workers, std::size_t channels>
void QNetwork<workers, channels>::printQNetwork() const {
    std::cout << "\nQNetwork\n";
    for (std::size_t i = 0U; i < numWorkers_; ++i) {
        std::cout << "Worker: " << i << "\n";
        
        std::cout << "Target: ";
        for (std::size_t j = vertexPointer_[i]; j < vertexPointer_[i + 1]; ++j) {
            const std::size_t tgt = edgeTargets_[j];
            std::cout << tgt;
            if (j < vertexPointer_[i + 1] - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";

        std::cout << "Multip: ";
        for (std::size_t j = vertexPointer_[i]; j < vertexPointer_[i + 1]; ++j) {
            const std::size_t mult = multiplicities_[j];
            std::cout << mult;
            if (j < vertexPointer_[i + 1] - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";

        std::cout << "Batchs: ";
        for (std::size_t j = vertexPointer_[i]; j < vertexPointer_[i + 1]; ++j) {
            const std::size_t batch = batchSize_[j];
            std::cout << batch;
            if (j < vertexPointer_[i + 1] - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "\n";
        std::cout << "\n";
    }
}

} // end namespace spapq