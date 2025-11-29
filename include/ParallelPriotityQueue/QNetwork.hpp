#pragma once

#include <algorithm>
#include <array>
#include <iostream>

namespace spapq {

template <std::size_t workers, std::size_t channels>
struct QNetwork {
    const std::size_t numWorkers_;
    const std::size_t numChannels_;
    std::size_t bufferSize_;
    std::size_t maxPushAttempts_;
    // Graph CSR
    std::array<std::size_t, workers + 1U> vertexPointer_;
    std::array<std::size_t, workers> numPorts_;
    std::array<std::size_t, channels> edgeTargets_;    // netw.numWorkers_ is reserved for efficient self-push
    std::array<std::size_t, channels> multiplicities_;
    std::array<std::size_t, channels> targetPort_;
    std::array<std::size_t, channels> batchSize_;

    constexpr void setDefaultMultiplicities();
    constexpr void setDefaultBatchSize();
    constexpr void setDefaultBufferSize();
    constexpr void setDefaultMaxPushAttempts();
    constexpr void assignTargetPorts();
    constexpr void changeToSelfPushLabels();

    constexpr bool isValidQNetwork() const;

    constexpr std::size_t maxBatchSize() const;
    constexpr std::size_t maxPortNum() const;

    constexpr bool hasHomogeneousMultiplicities() const;
    constexpr bool hasHomogeneousInPorts() const;
    constexpr bool hasHomogeneousOutPorts() const;
    constexpr bool hasHomogeneousPorts() const;
    constexpr bool hasHomogeneousBatchSize() const;

    void printQNetwork() const;

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets,
                       std::array<std::size_t, channels> multiplicities,
                       std::array<std::size_t, channels> batchSize,
                       std::size_t bufferSize,
                       std::size_t maxPushAttempts) :
        numWorkers_(workers),
        numChannels_(channels),
        bufferSize_(bufferSize),
        maxPushAttempts_(maxPushAttempts),
        vertexPointer_(vertexPointer),
        edgeTargets_(edgeTargets),
        multiplicities_(multiplicities),
        batchSize_(batchSize) {
        assignTargetPorts();
        changeToSelfPushLabels();
    };

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets,
                       std::array<std::size_t, channels> multiplicities,
                       std::array<std::size_t, channels> batchSize) :
        numWorkers_(workers),
        numChannels_(channels),
        vertexPointer_(vertexPointer),
        edgeTargets_(edgeTargets),
        multiplicities_(multiplicities),
        batchSize_(batchSize) {
        setDefaultBufferSize();
        setDefaultMaxPushAttempts();
        assignTargetPorts();
        changeToSelfPushLabels();
    };

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets,
                       std::array<std::size_t, channels> multiplicities) :
        numWorkers_(workers),
        numChannels_(channels),
        vertexPointer_(vertexPointer),
        edgeTargets_(edgeTargets),
        multiplicities_(multiplicities) {
        setDefaultBatchSize();
        setDefaultBufferSize();
        setDefaultMaxPushAttempts();
        assignTargetPorts();
        changeToSelfPushLabels();
    };

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets) :
        numWorkers_(workers), numChannels_(channels), vertexPointer_(vertexPointer), edgeTargets_(edgeTargets) {
        setDefaultMultiplicities();
        setDefaultBatchSize();
        setDefaultBufferSize();
        setDefaultMaxPushAttempts();
        assignTargetPorts();
        changeToSelfPushLabels();
    };
};

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultMultiplicities() {
    for (std::size_t i = 0U; i < multiplicities_.size(); ++i) { multiplicities_[i] = 1U; }
}

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultBatchSize() {
    for (std::size_t i = 0U; i < batchSize_.size(); ++i) { batchSize_[i] = 1U; }
}

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultBufferSize() {
    bufferSize_ = maxBatchSize() * 4U;
}

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultMaxPushAttempts() {
    maxPushAttempts_ = 4U;
}

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::assignTargetPorts() {
    for (std::size_t i = 0U; i < numPorts_.size(); ++i) { numPorts_[i] = 0U; }
    for (std::size_t worker = 0U; worker < numWorkers_; ++worker) {
        for (std::size_t edge = vertexPointer_[worker]; edge < vertexPointer_[worker + 1U]; ++edge) {
            if (edgeTargets_[edge] == numWorkers_) {
                targetPort_[edge] = numPorts_[worker]++;
            } else {
                targetPort_[edge] = numPorts_[edgeTargets_[edge]]++;
            }
        }
    }
}

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::changeToSelfPushLabels() {
    for (std::size_t worker = 0U; worker < numWorkers_; ++worker) {
        for (std::size_t edge = vertexPointer_[worker]; edge < vertexPointer_[worker + 1U]; ++edge) {
            if (edgeTargets_[edge] == worker) { edgeTargets_[edge] = numWorkers_; }
        }
    }
}

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::isValidQNetwork() const {
    if (numWorkers_ == 0U) { return false; }

    if (numChannels_ == 0U) { return false; }

    if (not std::all_of(edgeTargets_.cbegin(), edgeTargets_.cend(), [this](const std::size_t &tgt) {
            return tgt <= numWorkers_;
        })) {
        return false;
    }

    if (not std::all_of(multiplicities_.cbegin(), multiplicities_.cend(), [](const std::size_t &mul) {
            return mul > 0U;
        })) {
        return false;
    }

    if (not std::all_of(
            batchSize_.cbegin(), batchSize_.cend(), [](const std::size_t &batch) { return batch > 0U; })) {
        return false;
    }

    if (not std::all_of(
            numPorts_.cbegin(), numPorts_.cend(), [](const std::size_t &ports) { return ports > 0U; })) {
        return false;
    }

    for (std::size_t worker = 0U; worker < numWorkers_; ++worker) {
        if (vertexPointer_[worker] == vertexPointer_[worker + 1U]) { return false; }
    }

    if (bufferSize_ < maxBatchSize()) { return false; }
    if (maxPushAttempts_ == 0U) { return false; }

    return true;
}

template <std::size_t workers, std::size_t channels>
void QNetwork<workers, channels>::printQNetwork() const {
    std::cout << "\nQNetwork\n";
    for (std::size_t i = 0U; i < numWorkers_; ++i) {
        std::cout << "Worker: " << i << "\n";

        std::cout << "Target: ";
        for (std::size_t j = vertexPointer_[i]; j < vertexPointer_[i + 1U]; ++j) {
            const std::size_t tgt = edgeTargets_[j] == numWorkers_? i : edgeTargets_[j];
            std::cout << tgt;
            if (j < vertexPointer_[i + 1U] - 1U) { std::cout << ", "; }
        }
        std::cout << "\n";

        std::cout << "Multip: ";
        for (std::size_t j = vertexPointer_[i]; j < vertexPointer_[i + 1U]; ++j) {
            const std::size_t mult = multiplicities_[j];
            std::cout << mult;
            if (j < vertexPointer_[i + 1U] - 1U) { std::cout << ", "; }
        }
        std::cout << "\n";

        std::cout << "Batchs: ";
        for (std::size_t j = vertexPointer_[i]; j < vertexPointer_[i + 1U]; ++j) {
            const std::size_t batch = batchSize_[j];
            std::cout << batch;
            if (j < vertexPointer_[i + 1U] - 1U) { std::cout << ", "; }
        }
        std::cout << "\n";
        std::cout << "\n";
    }
}

template <std::size_t workers, std::size_t channels>
constexpr std::size_t QNetwork<workers, channels>::maxBatchSize() const {
    std::size_t max = 0U;
    for (std::size_t i = 0U; i < batchSize_.size(); ++i) { max = std::max(max, batchSize_[i]); }
    return max;
}

template <std::size_t workers, std::size_t channels>
constexpr std::size_t QNetwork<workers, channels>::maxPortNum() const {
    std::size_t max = 0U;
    for (std::size_t i = 0U; i < numPorts_.size(); ++i) { max = std::max(max, numPorts_[i]); }
    return max;
}

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::hasHomogeneousInPorts() const {
    if (numPorts_.size() == 0U) { return true; }
    const std::size_t ports = numPorts_[0U];
    for (std::size_t i = 0U; i < numPorts_.size(); ++i) {
        if (ports != numPorts_[i]) { return false; }
    }
    return true;
}

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::hasHomogeneousOutPorts() const {
    if (vertexPointer_.size() <= 1U) { return true; }
    const std::size_t ports = vertexPointer_[1U] - vertexPointer_[0U];
    for (std::size_t i = 0U; i < vertexPointer_.size() - 1; ++i) {
        if (ports != vertexPointer_[i + 1] - vertexPointer_[i]) { return false; }
    }
    return true;
}

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::hasHomogeneousPorts() const {
    bool returnValue = hasHomogeneousInPorts() && hasHomogeneousOutPorts();
    return returnValue;
}

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::hasHomogeneousBatchSize() const {
    if (batchSize_.size() == 0U) { return true; }
    std::size_t num = batchSize_[0U];
    for (std::size_t i = 0U; i < batchSize_.size(); ++i) {
        if (num != batchSize_[i]) { return false; }
    }
    return true;
}

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::hasHomogeneousMultiplicities() const {
    if (multiplicities_.size() == 0U) { return true; }
    std::size_t num = multiplicities_[0U];
    for (std::size_t i = 0U; i < multiplicities_.size(); ++i) {
        if (num != multiplicities_[i]) { return false; }
    }
    return true;
}

}    // end namespace spapq
