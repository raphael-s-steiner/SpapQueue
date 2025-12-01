#pragma once

#include "SpapQueueWorkerDeclaration.hpp"

namespace spapq {

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
template <std::size_t channelIndicesLength>
constexpr WorkerResource<GlobalQType, LocalQType, numPorts>::WorkerResource(
    GlobalQType &globalQueue, const std::array<std::size_t, channelIndicesLength> &channelIndices) :
    globalQueue_(globalQueue),
    channelIndices_(extendTable<maxTableSize(GlobalQType::netw_), channelIndicesLength>(channelIndices)) {
    bufferPointer_ = outBuffer_.begin();
    channelPointer_ = channelIndices_.cbegin();
    channelTableEndPointer_(std::next(channelIndices_.cbegin(), channelIndicesLength));
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
constexpr WorkerResource<GlobalQType, LocalQType, numPorts>::WorkerResource(GlobalQType &globalQueue,
                                                                            std::size_t workerId) :
    WorkerResource(globalQueue, qNetworkTable(GlobalQType::netw_, workerId)){};

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::push(const value_type &val, std::size_t port) {
    return inPorts_[port].push(val);
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::push(value_type &&val, std::size_t port) {
    return inPorts_[port].push(std::move(val));
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
template <class InputIt>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::push(InputIt first,
                                                                    InputIt last,
                                                                    std::size_t port) {
    return inPorts_[port].push(first, last);
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::enqueueGlobal(const value_type &val) {
    assert(bufferPointer_ != outBuffer_.end());

    globalQueue_.globalCount_.fetch_add(1U, std::memory_order_relaxed);
    *bufferPointer_ = val;
    ++bufferPointer_;

    std::size_t maxAttempts = GlobalQType::netw_.maxPushAttempts_;
    while (std::distance(outBuffer_.begin(), bufferPointer_) >= GlobalQType::netw_.batchSize_[*channelPointer_]
           && maxAttempts > 0U) {
        if (not pushOutBuffer()) { --maxAttempts; }

        ++channelPointer_;
        if (channelPointer_ == channelTableEndPointer_) { channelPointer_ = channelIndices_.cbegin(); }
    }
    if (maxAttempts == 0U) [[unlikely]] { pushOutBufferSelf(); }
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::pushOutBuffer() {
    bool successfulPush = false;

    const std::size_t targetWorker = GlobalQType::netw_.edgeTargets_[*channelPointer_];
    if (targetWorker == GlobalQType::netw_.numWorkers_) {    // netw.numWorkers_ is reserved for self-push
        pushOutBufferSelf();
        successfulPush = true;
    } else {
        const std::size_t port = GlobalQType::netw_.targetPort_[*channelPointer_];
        const std::size_t batch = GlobalQType::netw_.batchSize_[*channelPointer_];

        assert(batch <= std::distance(outBuffer_.begin(), bufferPointer_));
        const typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator itBegin
            = std::prev(bufferPointer_, batch);

        successfulPush = globalQueue_.pushInternal(itBegin, bufferPointer_, targetWorker, port);
        if (successfulPush) { bufferPointer_ = itBegin; }
    }

    return successfulPush;
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::pushOutBufferSelf() {
    constexpr bool hasBatchPush = requires (
        LocalQType &q,
        std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator first,
        std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator last) { q.push(first, last); };

    if constexpr (hasBatchPush) {
        queue_.push(outBuffer_.begin(), bufferPointer_);
    } else {
        for (auto it = outBuffer_.begin(); it != bufferPointer_; ++it) { queue_.push(*it); }
    }
    bufferPointer_ = outBuffer_.begin();
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::enqueueInChannels() {
    for (const auto &portRingBuffer : inPorts_) {
        std::optional<value_type> data = portRingBuffer.pop();
        while (data.has_value()) {
            queue_.push(data.value());
            data = portRingBuffer.pop();
        }
    }
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::run() {
    while (globalQueue_.globalCount_.load(std::memory_order_acquire) > 0) {
        while (not queue_.empty()) [[likely]] {
            enqueueInChannels();
            const value_type &val = queue_.top();
            processElement(val);
            queue_.pop();
            globalQueue_.globalCount_.fetch_sub(1U, std::memory_order_release);
        }
        enqueueInChannels();
        pushOutBufferSelf();
    }
}

}    // end namespace spapq
