#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>

namespace spapq {

/**
 * @brief A Network describing how the queue should be interlinked.
 *
 * @tparam workers Number of workers processing the queue.
 * @tparam channels Total number of channels between workers in the queue.
 */
template <std::size_t workers, std::size_t channels>
struct QNetwork {
    static constexpr std::size_t numWorkers_{workers};
    static constexpr std::size_t numChannels_{channels};
    std::size_t enqueueFrequency_;         ///< Number of tasks after which workers check incomming channels.
    std::size_t channelBufferSize_;        ///< Size or capacity of the RingBuffer channels.
    std::size_t maxPushAttempts_;          ///< Number of attempts to push to over channels before pushing to
                                           ///< self.
    std::array<std::size_t, workers + 1U> vertexPointer_;        ///< Vertex pointer in network CSR.
    std::array<std::size_t, workers> numPorts_;                  ///< Number of incomming channels of worker.
    std::array<std::size_t, workers> logicalCore_;               ///< Pthread core number of worker.
    std::array<std::size_t, channels> edgeTargets_;              ///< Target worker of channel in network CSR.
                                                                 ///< The number "numWorkers_" is reserved for
                                                                 ///< efficient self-push.
    std::array<std::size_t, channels> multiplicities_;           ///< How often this channel should be
                                                                 ///< preferred to push work over other
                                                                 ///< outgoing channels of the same worker.
    std::array<std::size_t, channels> targetPort_;        ///< Local index of channel of receiving worker.
    std::array<std::size_t, channels> batchSize_;         ///< Number of tasks to be pushed over a channel in
                                                          ///< one go.

    constexpr void setDefaultMultiplicities();
    constexpr void setDefaultBatchSize();
    constexpr void setDefaultChannelBufferSize();
    constexpr void setDefaultMaxPushAttempts();
    constexpr void setDefaultLogicalCores();
    constexpr void setDefaultEnqueueFrequency();

    constexpr void assignTargetPorts();
    constexpr void changeToSelfPushLabels();

    constexpr bool hasPathToAllWorkers(std::size_t worker) const;
    constexpr bool isStronglyConnected() const;
    constexpr bool isValidQNetwork() const;

    constexpr std::size_t maxBatchSize() const;
    constexpr std::size_t maxPortNum() const;

    constexpr bool hasHomogeneousMultiplicities() const;
    constexpr bool hasHomogeneousInPorts() const;
    constexpr bool hasHomogeneousOutPorts() const;
    constexpr bool hasHomogeneousPorts() const;
    constexpr bool hasHomogeneousBatchSize() const;
    constexpr bool hasSeparateLogicalCores() const;

    void printQNetwork() const;

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets,
                       std::array<std::size_t, workers> logicalCore,
                       std::array<std::size_t, channels> multiplicities,
                       std::array<std::size_t, channels> batchSize,
                       std::size_t enqueueFrequency,
                       std::size_t channelBufferSize,
                       std::size_t maxPushAttempts);

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets,
                       std::array<std::size_t, workers> logicalCore,
                       std::array<std::size_t, channels> multiplicities,
                       std::array<std::size_t, channels> batchSize);

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets,
                       std::array<std::size_t, workers> logicalCore,
                       std::array<std::size_t, channels> multiplicities);

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets,
                       std::array<std::size_t, workers> logicalCore);

    constexpr QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                       std::array<std::size_t, channels> edgeTargets);
};

// Implementation details

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultMultiplicities() {
    for (std::size_t i = 0U; i < multiplicities_.size(); ++i) { multiplicities_[i] = 1U; }
}

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultBatchSize() {
    for (std::size_t i = 0U; i < batchSize_.size(); ++i) { batchSize_[i] = 1U; }
}

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultEnqueueFrequency() {
    constexpr std::size_t numWorkersMininum1 = std::max(numWorkers_, static_cast<std::size_t>(1U));
    const std::size_t avgChannelNum = (numChannels_ + numWorkersMininum1 - 1U) / numWorkersMininum1;

    std::size_t pow2 = 1U;
    while (pow2 != 0U && pow2 < avgChannelNum) { pow2 *= 2U; }

    enqueueFrequency_ = std::max(static_cast<std::size_t>(16U), pow2 * 2U);
}

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultChannelBufferSize() {
    channelBufferSize_ = std::max(maxBatchSize() * 8U, enqueueFrequency_ * 4U);
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
        for (std::size_t channel = vertexPointer_[worker]; channel < vertexPointer_[worker + 1U]; ++channel) {
            const std::size_t tgt = edgeTargets_[channel] == numWorkers_ ? worker : edgeTargets_[channel];
            if (targetPort_.at(channel) >= numPorts_[tgt]) { return false; }
        }
    }

    for (std::size_t worker = 0U; worker < numWorkers_; ++worker) {
        std::array<bool, numChannels_> portOccupied;
        for (bool &val : portOccupied) { val = false; }

        for (std::size_t otherWorker = 0U; otherWorker < numWorkers_; ++otherWorker) {
            for (std::size_t channel = vertexPointer_[otherWorker]; channel < vertexPointer_[otherWorker + 1U];
                 ++channel) {
                const std::size_t tgt = edgeTargets_[channel] == numWorkers_ ? otherWorker :
                                                                               edgeTargets_[channel];

                if (tgt == worker) {
                    if (portOccupied[targetPort_[channel]]) { return false; }
                    portOccupied[targetPort_[channel]] = true;
                }
            }
        }
    }

    for (std::size_t worker = 0U; worker < numWorkers_; ++worker) {
        if (vertexPointer_[worker] == vertexPointer_[worker + 1U]) { return false; }
    }

    if (channelBufferSize_ < maxBatchSize()) { return false; }
    if (maxPushAttempts_ == 0U) { return false; }
    if (enqueueFrequency_ == 0U) { return false; }

    return true;
}

template <std::size_t workers, std::size_t channels>
void QNetwork<workers, channels>::printQNetwork() const {
    const std::string singleIndent = " ";
    const std::string doubleIndent = singleIndent + singleIndent;

    std::cout << "\nQNetwork:\n";
    std::cout << singleIndent << "#Workers : " << numWorkers_ << "\n";
    std::cout << singleIndent << "#Channels: " << numChannels_ << "\n";
    std::cout << singleIndent << "EnQFreq  : " << enqueueFrequency_ << "\n";
    std::cout << singleIndent << "ChanlSize: " << channelBufferSize_ << "\n";
    std::cout << singleIndent << "MaxAttmps: " << maxPushAttempts_ << "\n";

    std::cout << "\n" << singleIndent << "Linking:\n";
    for (std::size_t i = 0U; i < numWorkers_; ++i) {
        std::cout << doubleIndent << "Worker: " << i << "\n";
        std::cout << doubleIndent << "Core  : " << logicalCore_[i] << "\n";

        std::cout << doubleIndent << "Target: ";
        for (std::size_t j = vertexPointer_[i]; j < vertexPointer_[i + 1U]; ++j) {
            const std::size_t tgt = edgeTargets_[j] == numWorkers_ ? i : edgeTargets_[j];
            std::cout << tgt;
            if (j < vertexPointer_[i + 1U] - 1U) { std::cout << ", "; }
        }
        std::cout << "\n";

        std::cout << doubleIndent << "Multip: ";
        for (std::size_t j = vertexPointer_[i]; j < vertexPointer_[i + 1U]; ++j) {
            const std::size_t mult = multiplicities_[j];
            std::cout << mult;
            if (j < vertexPointer_[i + 1U] - 1U) { std::cout << ", "; }
        }
        std::cout << "\n";

        std::cout << doubleIndent << "Batchs: ";
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

template <std::size_t workers, std::size_t channels>
constexpr void QNetwork<workers, channels>::setDefaultLogicalCores() {
    for (std::size_t i = 0U; i < logicalCore_.size(); ++i) { logicalCore_[i] = i; }
}

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::hasSeparateLogicalCores() const {
    std::array<std::size_t, workers> logicalCoresCopy = logicalCore_;
    std::sort(logicalCoresCopy.begin(), logicalCoresCopy.end());

    for (std::size_t i = 0U; i < logicalCoresCopy.size() - 1U; ++i) {
        if (logicalCoresCopy[i] == logicalCoresCopy[i + 1]) { return false; }
    }

    return true;
}

template <std::size_t workers, std::size_t channels>
constexpr QNetwork<workers, channels>::QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                                                std::array<std::size_t, channels> edgeTargets,
                                                std::array<std::size_t, workers> logicalCore,
                                                std::array<std::size_t, channels> multiplicities,
                                                std::array<std::size_t, channels> batchSize,
                                                std::size_t enqueueFrequency,
                                                std::size_t channelBufferSize,
                                                std::size_t maxPushAttempts) :
    enqueueFrequency_(enqueueFrequency),
    channelBufferSize_(channelBufferSize),
    maxPushAttempts_(maxPushAttempts),
    vertexPointer_(vertexPointer),
    logicalCore_(logicalCore),
    edgeTargets_(edgeTargets),
    multiplicities_(multiplicities),
    batchSize_(batchSize) {
    assignTargetPorts();
    changeToSelfPushLabels();
};

template <std::size_t workers, std::size_t channels>
constexpr QNetwork<workers, channels>::QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                                                std::array<std::size_t, channels> edgeTargets,
                                                std::array<std::size_t, workers> logicalCore,
                                                std::array<std::size_t, channels> multiplicities,
                                                std::array<std::size_t, channels> batchSize) :
    vertexPointer_(vertexPointer),
    logicalCore_(logicalCore),
    edgeTargets_(edgeTargets),
    multiplicities_(multiplicities),
    batchSize_(batchSize) {
    setDefaultEnqueueFrequency();
    setDefaultChannelBufferSize();
    setDefaultMaxPushAttempts();
    assignTargetPorts();
    changeToSelfPushLabels();
};

template <std::size_t workers, std::size_t channels>
constexpr QNetwork<workers, channels>::QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                                                std::array<std::size_t, channels> edgeTargets,
                                                std::array<std::size_t, workers> logicalCore,
                                                std::array<std::size_t, channels> multiplicities) :
    vertexPointer_(vertexPointer),
    logicalCore_(logicalCore),
    edgeTargets_(edgeTargets),
    multiplicities_(multiplicities) {
    setDefaultBatchSize();
    setDefaultEnqueueFrequency();
    setDefaultChannelBufferSize();
    setDefaultMaxPushAttempts();
    assignTargetPorts();
    changeToSelfPushLabels();
};

template <std::size_t workers, std::size_t channels>
constexpr QNetwork<workers, channels>::QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                                                std::array<std::size_t, channels> edgeTargets,
                                                std::array<std::size_t, workers> logicalCore) :
    vertexPointer_(vertexPointer), logicalCore_(logicalCore), edgeTargets_(edgeTargets) {
    setDefaultMultiplicities();
    setDefaultBatchSize();
    setDefaultEnqueueFrequency();
    setDefaultChannelBufferSize();
    setDefaultMaxPushAttempts();
    assignTargetPorts();
    changeToSelfPushLabels();
};

template <std::size_t workers, std::size_t channels>
constexpr QNetwork<workers, channels>::QNetwork(std::array<std::size_t, workers + 1U> vertexPointer,
                                                std::array<std::size_t, channels> edgeTargets) :
    vertexPointer_(vertexPointer), edgeTargets_(edgeTargets) {
    setDefaultLogicalCores();
    setDefaultMultiplicities();
    setDefaultBatchSize();
    setDefaultEnqueueFrequency();
    setDefaultChannelBufferSize();
    setDefaultMaxPushAttempts();
    assignTargetPorts();
    changeToSelfPushLabels();
};

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::hasPathToAllWorkers(std::size_t worker) const {
    assert(worker < numWorkers_);

    std::array<bool, numWorkers_> reachable;
    for (bool &val : reachable) { val = false; }
    for (std::size_t channel = vertexPointer_[worker]; channel < vertexPointer_[worker + 1U]; ++channel) {
        const std::size_t tgt = edgeTargets_[channel] == numWorkers_ ? worker : edgeTargets_[channel];
        reachable[tgt] = true;
    }

    bool hasChanged = true;
    while (hasChanged) {
        hasChanged = false;

        for (std::size_t w = 0U; w < numWorkers_; ++w) {
            if (!reachable[w]) { continue; }

            for (std::size_t channel = vertexPointer_[w]; channel < vertexPointer_[w + 1U]; ++channel) {
                const std::size_t tgt = edgeTargets_[channel] == numWorkers_ ? w : edgeTargets_[channel];
                if (reachable[tgt]) { continue; }

                reachable[tgt] = true;
                hasChanged = true;
            }
        }
    }

    return std::all_of(reachable.cbegin(), reachable.cend(), [](bool val) { return val; });
}

template <std::size_t workers, std::size_t channels>
constexpr bool QNetwork<workers, channels>::isStronglyConnected() const {
    for (std::size_t worker = 0U; worker < numWorkers_; ++worker) {
        if (not hasPathToAllWorkers(worker)) { return false; }
    }
    return true;
}

}        // end namespace spapq
