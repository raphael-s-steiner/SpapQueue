#pragma once

#include <iterator>
#include <memory>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "ParallelPriotityQueue/QNetwork.hpp"
#include "RingBuffer/RingBuffer.hpp"

namespace spapq {

template <typename T,
          QNetwork<workers, channels> netw,
          typename LocalQType = std::priority_queue<T>>
class SpapQueue {
  public:
    using GlobalQType = SpapQueue<T, netw, LocalQType>;

    template <std::size_t numPorts, std::size_t tableLength>
    class WorkerResource {
        template <std::size_t, std::size_t>
        friend class WorkerResource;
        friend class GlobalQType;

      private:
        const std::size_t workerId_;
        GlobalQType &globalQueue_;
        std::array<T, netw.maxBatchSize()>::iterator bufferPointer_;
        std::array<std::size_t, tableLength>::const_iterator channelPointer_;
        // const std::array<std::size_t, tableLength>::const_iterator channelTableEndPointer_;
        std::array<T, netw.maxBatchSize()> outBuffer_;
        const std::array<std::size_t, tableLength> channelIndices_;
        std::array<RingBuffer<T, netw.bufferSize_>, numPorts> inPorts_;
        LocalQType queue_;

        [[nodiscard("Push may fail when queue is full")]] inline bool pushOutBuffer();
        inline void pushOutBufferSelf();

        inline void enqueueInChannels();
        virtual inline void processElement(T &&val) = 0;

        [[nodiscard("Push may fail when queue is full")]] inline bool push(const T &val,
                                                                           std::size_t port) {
            return inPorts_[port].push(val);
        };

        [[nodiscard("Push may fail when queue is full")]] inline bool push(T &&val,
                                                                           std::size_t port) {
            return inPorts_[port].push(std::move(val));
        };

        template <class InputIt>
        [[nodiscard("Push may fail when queue is full")]] inline bool push(InputIt first,
                                                                           InputIt last,
                                                                           std::size_t port) {
            return inPorts_[port].push(first, last);
        }

      protected:
        inline void enqueueGlobal(const T &val);

      public:
        WorkerResource(std::size_t workerId,
                       GlobalQType &globalQueue,
                       const std::array<std::size_t, tableLength> channelIndices) :
            workerId_(workerId),
            globalQueue_(globalQueue),
            bufferPointer_(outBuffer.begin()),
            channelPointer_(channelIndices_.begin()),
            channelIndices_(std::move(channelIndices)) { };
        WorkerResource(const WorkerResource &other) = 0;
        WorkerResource(WorkerResource &&other) = 0;
        WorkerResource &operator=(const WorkerResource &other) = 0;
        WorkerResource &operator=(WorkerResource &&other) = 0;
        ~WorkerResource() = default;

        inline void run();
    };

    template <std::size_t N>
    struct WorkerCollectiveHelper {
        static_assert(N <= netw.numWorkers_);
        template <typename... Args>
        using partialType = typename WorkerCollectiveHelper<N - 1>::template partialType<
            std::unique_ptr<WorkerResource<netw.numPorts_[N - 1],
                                           sumArray(qNetworkTableFrequencies(netw, N - 1))>>,
            Args...>;
    };

    template <>
    struct WorkerCollectiveHelper<0> {
        template <typename... Args>
        using partialType = std::tuple<Args...>;
    };

    using WorkerCollective = std::conditional_t<
        netw.hasHomogeneousInPorts(),
        std::array<std::unique_ptr<WorkerResource<netw.numPorts_[0U], maxTableSize(netw)>>,
                   netw.numWorkers_>,
        typename WorkerCollectiveHelper<netw.numWorkers_>::template partialType<>>;

    alignas(CACHE_LINE_SIZE)
        std::array<std::jthread, netw.numWorkers_> workers_;    // TODO does not need to be aligned
    alignas(CACHE_LINE_SIZE) WorkerCollective workerResources_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> globalCount_{0U};
    std::atomic_flag initSync;

    void initQueue();
    void pushUnsafe(const T &val, const std::size_t workerId = 0U);
    void pushUnsafe(T &&val, const std::size_t workerId = 0U);

    void processQueue() {
        initSynch.test_and_set(std::memory_order_release);
        initSynch.notify_all();
    };

    void waitProcessFinish();

    template <std::size_t tupleSize,
              std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternalHelper(
        const T &val, const std::size_t workerId, const std::size_t port);
    template <std::size_t tupleSize,
              std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternalHelper(
        T &&val, const std::size_t workerId, const std::size_t port);
    template <std::size_t tupleSize,
              class InputIt,
              std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternalHelper(
        InputIt first, InputIt last, const std::size_t workerId, const std::size_t port);

    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternal(
        const T &val, const std::size_t workerId, const std::size_t port);
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternal(
        T &&val, const std::size_t workerId, const std::size_t port);
    template <class InputIt>
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternal(
        InputIt first, InputIt last, const std::size_t workerId, const std::size_t port);

    static_assert(netw.isValidQNetwork(), "The QNetwork needs to be valid!");
    static_assert(std::is_same_v<T, typename LocalQType::value_type>,
                  "The Local Queue Type needs to have matching value_type!");
};

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline void SpapQueue<T, netw, LocalQType>::WorkerResource<numPorts, tableLength>::enqueueGlobal(
    const T &val) {
    assert(bufferPointer_ != outBuffer_.end());

    globalQueue_.globalCount_.fetch_add(1U, std::memory_order_relaxed);
    *bufferPointer_ = val;
    ++bufferPointer_;

    std::size_t maxAttempts = netw.maxPushAttempts_;
    while (std::distance(outBuffer_.begin(), bufferPointer_) >= netw.batchSize_[*channelPointer]
           && maxAttempts > 0U) {
        if (not pushOutBuffer()) { --maxAttempts; }

        ++channelPointer_;
        if (channelPointer_ == channelIndices_.cend()) {
            channelPointer_ = channelIndices_.cbegin();
        }
    }
    if (maxAttempts == 0U) [[unlikely]] { pushOutBufferSelf(); }
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline bool SpapQueue<T, netw, LocalQType>::WorkerResource<numPorts, tableLength>::pushOutBuffer() {
    bool successfulPush = false;

    const std::size_t targetWorker = netw.edgeTargets_[*channelPointer_];
    if (targetWorker == workerId_) {
        pushOutBufferSelf();
        successfulPush = true;
    } else {
        const std::size_t port = netw.targetPort_[*channelPointer_];
        const std::size_t batch = netw.batchSize_[*channelPointer_];

        assert(batch <= std::distance(outBuffer_.begin(), bufferPointer_));
        const std::array<T, netw.maxBatchSize()>::iterator itBegin = bufferPointer_;
        std::prev(itBegin, batch);

        successfulPush = globalQueue_.pushInternal(itBegin, bufferPointer_, targetWorker, port);
        if (successfulPush) { bufferPointer_ = itBegin; }
    }

    return successfulPush;
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline void SpapQueue<T, netw, LocalQType>::WorkerResource<numPorts,
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

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline void SpapQueue<T, netw, LocalQType>::WorkerResource<numPorts,
                                                           tableLength>::enqueueInChannels() {
    for (const auto &portRingBuffer : inPorts_) {
        std::optional<T> data = portRingBuffer.pop();
        while (data.has_value()) {
            queue_.push(data.value());
            data = portRingBuffer.pop();
        }
    }
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t numPorts, std::size_t tableLength>
inline void SpapQueue<T, netw, LocalQType>::WorkerResource<numPorts, tableLength>::run() {
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

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
void SpapQueue<T, netw, LocalQType>::waitProcessFinish() {
    for (auto &thread : workers_) { thread.join(); }
    initSync.clear();
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t tupleSize, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
inline bool SpapQueue<T, netw, LocalQType>::pushInternalHelper<tupleSize, true>(
    const T &val, const std::size_t workerId, const std::size_t port) {
    static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
    if (workerId == (std::tuple_size_v(workerResources_) - tupleSize)) {
        return std::get<std::tuple_size_v(workerResources_) - tupleSize>(workerResources_)
            .push(val, port);
    } else {
        if constexpr (tupleSize > 1) {
            return pushInternalHelper<tupleSize - 1>(val, workerId, port);
        } else {
#ifdef __cpp_lib_unreachable
            std::unreachable();
#endif
            assert(false);
            return false;
        }
    }
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t tupleSize, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
inline bool SpapQueue<T, netw, LocalQType>::pushInternalHelper<tupleSize, true>(
    T &&val, const std::size_t workerId, const std::size_t port) {
    static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
    if (workerId == (std::tuple_size_v(workerResources_) - tupleSize)) {
        return std::get<std::tuple_size_v(workerResources_) - tupleSize>(workerResources_)
            .push(std::move(val), port);
    } else {
        if constexpr (tupleSize > 1) {
            return pushInternalHelper<tupleSize - 1>(std::move(val), workerId, port);
        } else {
#ifdef __cpp_lib_unreachable
            std::unreachable();
#endif
            assert(false);
            return false;
        }
    }
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t tupleSize,
          class InputIt,
          std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
inline bool SpapQueue<T, netw, LocalQType>::pushInternalHelper<tupleSize, true>(
    InputIt first, InputIt last, const std::size_t workerId, const std::size_t port) {
    static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
    if (workerId == (std::tuple_size_v(workerResources_) - tupleSize)) {
        return std::get<std::tuple_size_v(workerResources_) - tupleSize>(workerResources_)
            .push(first, last, port);
    } else {
        if constexpr (tupleSize > 1) {
            return pushInternalHelper<tupleSize - 1, InputIt>(first, last, workerId, port);
        } else {
#ifdef __cpp_lib_unreachable
            std::unreachable();
#endif
            assert(false);
            return false;
        }
    }
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
inline bool SpapQueue<T, netw, LocalQType>::pushInternal(const T &val,
                                                         const std::size_t workerId,
                                                         const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(val, port);
    } else {
        return pushInternalHelper<netw.numWorkers_>(val, workerId, port);
    }
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
inline bool SpapQueue<T, netw, LocalQType>::pushInternal(T &&val,
                                                         const std::size_t workerId,
                                                         const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(std::move(val), port);
    } else {
        return pushInternalHelper<netw.numWorkers_>(std::move(val), workerId, port);
    }
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
template <class InputIt>
inline bool SpapQueue<T, netw, LocalQType>::pushInternal(InputIt first,
                                                         InputIt last,
                                                         const std::size_t workerId,
                                                         const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(first, last, port);
    } else {
        return pushInternalHelper<netw.numWorkers_, InputIt>(first, last, workerId, port);
    }
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
void SpapQueue<T, netw, LocalQType>::pushUnsafe(const T &val, const std::size_t workerId) {
    // TODO
}

template <typename T, QNetwork<workers, channels> netw, typename LocalQType>
void SpapQueue<T, netw, LocalQType>::pushUnsafe(T &&val, const std::size_t workerId) {
    // TODO
}

}    // end namespace spapq
