#pragma once

#include <iterator>

#include "RingBuffer/RingBuffer.hpp"
#include "SpapQueueDeclaration.hpp"

namespace spapq {

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
class SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource {
    template <std::size_t, std::size_t>
    friend class WorkerResource;
    friend class SpapQueue<T, workers, channels, netw, LocalQType>;

  private:
    SpapQueue<T, workers, channels, netw, LocalQType> &globalQueue_;
    std::array<T, netw.maxBatchSize()>::iterator bufferPointer_;
    std::array<std::size_t, tableLength>::const_iterator channelPointer_;
    const std::array<std::size_t, tableLength>::const_iterator channelTableEndPointer_;
    std::array<T, netw.maxBatchSize()> outBuffer_;
    const std::array<std::size_t, tableLength> channelIndices_;
    std::array<RingBuffer<T, netw.bufferSize_>, numPorts> inPorts_;
    LocalQType queue_;

    [[nodiscard("Push may fail when queue is full")]] inline bool pushOutBuffer();
    inline void pushOutBufferSelf();

    inline void enqueueInChannels();
    inline void processElement(T &&val);

    [[nodiscard("Push may fail when queue is full")]] inline bool push(const T &val, std::size_t port);
    [[nodiscard("Push may fail when queue is full")]] inline bool push(T &&val, std::size_t port);
    template <class InputIt>
    [[nodiscard("Push may fail when queue is full")]] inline bool push(InputIt first,
                                                                       InputIt last,
                                                                       std::size_t port);

  protected:
    inline void enqueueGlobal(const T &val);

  public:
    WorkerResource(SpapQueue<T, workers, channels, netw, LocalQType> &globalQueue,
                   const std::array<std::size_t, tableLength> channelIndices,
                   std::array<std::size_t, tableLength>::const_iterator channelTableEndPointer) :
        globalQueue_(globalQueue),
        bufferPointer_(outBuffer_.begin()),
        channelPointer_(channelIndices_.begin()),
        channelTableEndPointer_(channelTableEndPointer),
        channelIndices_(std::move(channelIndices)) { };
    WorkerResource(const WorkerResource &other) = delete;
    WorkerResource(WorkerResource &&other) = delete;
    WorkerResource &operator=(const WorkerResource &other) = delete;
    WorkerResource &operator=(WorkerResource &&other) = delete;
    ~WorkerResource() = default;

    inline void run();
};

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline bool SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource<numPorts, tableLength>::push(
    const T &val, std::size_t port) {
    return inPorts_[port].push(val);
};

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline bool SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource<numPorts, tableLength>::push(
    T &&val, std::size_t port) {
    return inPorts_[port].push(std::move(val));
};

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
template <class InputIt>
inline bool SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource<numPorts, tableLength>::push(
    InputIt first, InputIt last, std::size_t port) {
    return inPorts_[port].push(first, last);
}

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline void SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource<numPorts, tableLength>::enqueueGlobal(
    const T &val) {
    assert(bufferPointer_ != outBuffer_.end());

    globalQueue_.globalCount_.fetch_add(1U, std::memory_order_relaxed);
    *bufferPointer_ = val;
    ++bufferPointer_;

    std::size_t maxAttempts = netw.maxPushAttempts_;
    while (std::distance(outBuffer_.begin(), bufferPointer_) >= netw.batchSize_[*channelPointer_]
           && maxAttempts > 0U) {
        if (not pushOutBuffer()) { --maxAttempts; }

        ++channelPointer_;
        if (channelPointer_ == channelIndices_.cend()) { channelPointer_ = channelIndices_.cbegin(); }
    }
    if (maxAttempts == 0U) [[unlikely]] { pushOutBufferSelf(); }
}

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline bool SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource<numPorts,
                                                                              tableLength>::pushOutBuffer() {
    bool successfulPush = false;

    const std::size_t targetWorker = netw.edgeTargets_[*channelPointer_];
    if (targetWorker == netw.numWorkers_) {    // netw.numWorkers_ is reserved for self-push
        pushOutBufferSelf();
        successfulPush = true;
    } else {
        const std::size_t port = netw.targetPort_[*channelPointer_];
        const std::size_t batch = netw.batchSize_[*channelPointer_];

        assert(batch <= std::distance(outBuffer_.begin(), bufferPointer_));
        const typename std::array<T, netw.maxBatchSize()>::iterator itBegin = bufferPointer_;
        std::prev(itBegin, batch);

        successfulPush = globalQueue_.pushInternal(itBegin, bufferPointer_, targetWorker, port);
        if (successfulPush) { bufferPointer_ = itBegin; }
    }

    return successfulPush;
}

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline void SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource<numPorts,
                                                                              tableLength>::pushOutBufferSelf() {
    constexpr bool hasBatchPush
        = requires (LocalQType &q,
                    std::array<T, netw.maxBatchSize()>::iterator first,
                    std::array<T, netw.maxBatchSize()>::iterator last) { q.push(first, last); };

    if constexpr (hasBatchPush) {
        queue_.push(outBuffer_.begin(), bufferPointer_);
    } else {
        for (auto it = outBuffer_.begin(); it != bufferPointer_; ++it) { queue_.push(*it); }
    }
    bufferPointer_ = outBuffer_.begin();
}

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline void SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource<numPorts,
                                                                              tableLength>::enqueueInChannels() {
    for (const auto &portRingBuffer : inPorts_) {
        std::optional<T> data = portRingBuffer.pop();
        while (data.has_value()) {
            queue_.push(data.value());
            data = portRingBuffer.pop();
        }
    }
}

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline void SpapQueue<T, workers, channels, netw, LocalQType>::WorkerResource<numPorts, tableLength>::run() {
    while (globalQueue_.globalCount_.load(std::memory_order_acquire) > 0) {
        while (not queue_.empty()) [[likely]] {
            enqueueInChannels();
            T val = queue_.top();
            processElement(std::move(val));
            queue_.pop();
            globalQueue_.globalCount_.fetch_sub(1U, std::memory_order_release);
        }
        enqueueInChannels();
        pushOutBufferSelf();
    }
}

}    // end namespace spapq
