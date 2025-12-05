#pragma once

#include <barrier>
#include <memory>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

#include "Discrepancy/QNetworkTables.hpp"
#include "Discrepancy/TableGenerator.hpp"
#include "SpapQueueWorker.hpp"

namespace spapq {

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
class SpapQueue {
    using ThisQType = SpapQueue<T, netw, WorkerTemplate, LocalQType>;

    template <typename, typename, std::size_t>
    friend class WorkerTemplate;

  public:
    using value_type = T;

    using WorkerCollective = std::conditional_t<
        netw.hasHomogeneousInPorts(),
        std::array<WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[0U]> *, netw.numWorkers_>,
        typename WorkerCollectiveHelper<WorkerTemplate, ThisQType, LocalQType, netw, netw.numWorkers_>::template type<>
    >;

    static constexpr QNetwork<netw.numWorkers_, netw.numChannels_> netw_{netw};

    alignas(CACHE_LINE_SIZE)
        std::array<std::jthread, netw.numWorkers_> workers_;    // TODO does not need to be aligned
    alignas(CACHE_LINE_SIZE) WorkerCollective workerResources_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> globalCount_{0U};
    std::atomic_flag startSignal;
    std::barrier<> allocateSignal{netw.numWorkers_ + 1};
    std::barrier<> safeToDeallocateSignal{netw.numWorkers_};

    void initQueue();
    void pushUnsafe(const value_type &val, const std::size_t workerId = 0U);
    void pushUnsafe(value_type &&val, const std::size_t workerId = 0U);

    void processQueue();

    void waitProcessFinish();

    template <std::size_t tupleSize,
              bool networkHomogeneousInPorts = netw.hasHomogeneousInPorts(),
              std::enable_if_t<not networkHomogeneousInPorts, bool> = true>
    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternalHelper(
        const value_type &val, const std::size_t workerId, const std::size_t port);

    template <std::size_t tupleSize,
              bool networkHomogeneousInPorts = netw.hasHomogeneousInPorts(),
              std::enable_if_t<not networkHomogeneousInPorts, bool> = true>
    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternalHelper(
        value_type &&val, const std::size_t workerId, const std::size_t port);

    template <std::size_t tupleSize,
              class InputIt,
              bool networkHomogeneousInPorts = netw.hasHomogeneousInPorts(),
              std::enable_if_t<not networkHomogeneousInPorts, bool> = true>
    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternalHelper(
        InputIt first, InputIt last, const std::size_t workerId, const std::size_t port);

    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternal(const value_type &val,
                                                                                const std::size_t workerId,
                                                                                const std::size_t port);

    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternal(value_type &&val,
                                                                                const std::size_t workerId,
                                                                                const std::size_t port);

    template <class InputIt>
    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternal(InputIt first,
                                                                                InputIt last,
                                                                                const std::size_t workerId,
                                                                                const std::size_t port);

    template <std::size_t N>
    void threadWork();

    static_assert(netw.isValidQNetwork(), "The QNetwork needs to be valid!");
    static_assert(std::is_same_v<value_type, typename LocalQType::value_type>,
                  "The local queue type needs to have matching value_type!");
    static_assert(isDerivedWorkerResource<WorkerTemplate, ThisQType, LocalQType, netw.numWorkers_>(),
                  "WorkerTemplate must be derived from WorkerResource.");
};

// Implementation Details

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::waitProcessFinish() {
    for (auto &thread : workers_) { thread.join(); }
    startSignal.clear();
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::processQueue() {
    startSignal.test_and_set(std::memory_order_release);
    startSignal.notify_all();
};

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t tupleSize,
          bool networkHomogeneousInPorts,
          std::enable_if_t<not networkHomogeneousInPorts, bool>>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternalHelper(const T &val,
                                                                               const std::size_t workerId,
                                                                               const std::size_t port) {
    static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
    if constexpr (tupleSize == netw.numWorkers_) { assert(workerId < netw.numWorkers_); }

    if (workerId == netw.numWorkers_ - tupleSize) {
        return std::get<netw.numWorkers_ - tupleSize>(workerResources_).push(val, port);
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

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t tupleSize,
          bool networkHomogeneousInPorts,
          std::enable_if_t<not networkHomogeneousInPorts, bool>>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternalHelper(T &&val,
                                                                               const std::size_t workerId,
                                                                               const std::size_t port) {
    static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
    if constexpr (tupleSize == netw.numWorkers_) { assert(workerId < netw.numWorkers_); }

    if (workerId == (netw.numWorkers_ - tupleSize)) {
        return std::get<netw.numWorkers_ - tupleSize>(workerResources_).push(std::move(val), port);
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

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t tupleSize,
          class InputIt,
          bool networkHomogeneousInPorts,
          std::enable_if_t<not networkHomogeneousInPorts, bool>>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternalHelper(InputIt first,
                                                                               InputIt last,
                                                                               const std::size_t workerId,
                                                                               const std::size_t port) {
    static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
    if constexpr (tupleSize == netw.numWorkers_) { assert(workerId < netw.numWorkers_); }

    if (workerId == (netw.numWorkers_ - tupleSize)) {
        return std::get<netw.numWorkers_ - tupleSize>(workerResources_).push(first, last, port);
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

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternal(const T &val,
                                                                         const std::size_t workerId,
                                                                         const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(val, port);
    } else {
        return pushInternalHelper<netw.numWorkers_>(val, workerId, port);
    }
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternal(T &&val,
                                                                         const std::size_t workerId,
                                                                         const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(std::move(val), port);
    } else {
        return pushInternalHelper<netw.numWorkers_>(std::move(val), workerId, port);
    }
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <class InputIt>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternal(InputIt first,
                                                                         InputIt last,
                                                                         const std::size_t workerId,
                                                                         const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(first, last, port);
    } else {
        return pushInternalHelper<netw.numWorkers_, InputIt>(first, last, workerId, port);
    }
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t N>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::threadWork() {
    static_assert(N < netw.numWorkers_);

    // init resource
    WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[N]> resource(*this, N);

    // set reference
    if constexpr (netw.hasHomogeneousInPorts()) {
        workerResources_[N] = &resource;
    } else {
        std::get<N>(workerResources_) = &resource;
    }

    // signal reference set
    allocateSignal.arrive_and_wait();

    // awaiting unsafe enqueuing and global starting signal
    startSignal.wait(false, std::memory_order_acquire);

    // run
    resource.run();

    // signal and await process finished
    safeToDeallocateSignal.arrive_and_wait();

    // unset reference
    if constexpr (netw.hasHomogeneousInPorts()) {
        workerResources_[N] = nullptr;
    } else {
        std::get<N>(workerResources_) = nullptr;
    }
}

// template <typename T,
//           QNetwork netw,
//           template <class, class, std::size_t> class WorkerTemplate,
//           typename LocalQType>
// void SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushUnsafe(const T &val, const
// std::size_t workerId) {
//     // TODO
// }

// template <typename T,
//           QNetwork netw,
//           template <class, class, std::size_t> class WorkerTemplate,
//           typename LocalQType>
// void SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushUnsafe(T &&val, const
// std::size_t workerId) {
//     // TODO
// }

}    // end namespace spapq
