#pragma once

#include <pthread.h>

#include <barrier>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
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
    template <typename, typename, std::size_t>
    friend class WorkerTemplate;
    template <typename, typename, std::size_t>
    friend class WorkerResource;

  public:
    using value_type = T;
    static constexpr QNetwork<netw.numWorkers_, netw.numChannels_> netw_{netw};

    void initQueue();
    void processQueue();
    void waitProcessFinish();

    void pushUnsafe(const value_type &val, const std::size_t workerId = 0U);
    void pushUnsafe(value_type &&val, const std::size_t workerId = 0U);

  private:
    using ThisQType = SpapQueue<T, netw, WorkerTemplate, LocalQType>;

    using WorkerCollective = std::conditional_t<
        netw.hasHomogeneousInPorts(),
        std::array<WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[0U]> *, netw.numWorkers_>,
        typename WorkerCollectiveHelper<WorkerTemplate, ThisQType, LocalQType, netw, netw.numWorkers_>::template type<>
    >;

    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> globalCount_{0U};
    alignas(CACHE_LINE_SIZE) WorkerCollective workerResources_;

    bool queueActive_{false};
    std::atomic_flag startSignal_;
    std::barrier<> allocateSignal_{netw.numWorkers_ + 1};
    std::barrier<> safeToDeallocateSignal_{netw.numWorkers_};

    std::array<std::jthread, netw.numWorkers_> workers_;

    template <std::size_t N>
    void threadWork(std::stop_token stoken);

    // [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternal(const value_type &val,
    //                                                                             const std::size_t workerId,
    //                                                                             const std::size_t port);

    // [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternal(value_type &&val,
    //                                                                             const std::size_t workerId,
    //                                                                             const std::size_t port);

    template <class InputIt>
    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternal(InputIt first,
                                                                                InputIt last,
                                                                                const std::size_t workerId,
                                                                                const std::size_t port);

    // Helper Functions
    // template <std::size_t tupleSize,
    //           bool networkHomogeneousInPorts = netw.hasHomogeneousInPorts(),
    //           std::enable_if_t<not networkHomogeneousInPorts, bool> = true>
    // [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternalHelper(
    //     const value_type &val, const std::size_t workerId, const std::size_t port);

    // template <std::size_t tupleSize,
    //           bool networkHomogeneousInPorts = netw.hasHomogeneousInPorts(),
    //           std::enable_if_t<not networkHomogeneousInPorts, bool> = true>
    // [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternalHelper(
    //     value_type &&val, const std::size_t workerId, const std::size_t port);

    template <std::size_t tupleSize,
              class InputIt,
              bool networkHomogeneousInPorts = netw.hasHomogeneousInPorts(),
              std::enable_if_t<not networkHomogeneousInPorts, bool> = true>
    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternalHelper(
        InputIt first, InputIt last, const std::size_t workerId, const std::size_t port);

    // Static asserts
    static_assert(netw.isValidQNetwork(), "The QNetwork needs to be valid!");
    static_assert(std::is_same_v<value_type, typename LocalQType::value_type>,
                  "The local queue type needs to have matching value_type!");
    static_assert(isDerivedWorkerResource<WorkerTemplate, ThisQType, LocalQType, netw.numWorkers_>(),
                  "WorkerTemplate must be derived from WorkerResource.");
    static_assert(netw.hasSeparateLogicalCores(), "Workers should be on separate logical Cores.");
};

// Implementation Details

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::waitProcessFinish() {
    for (auto &thread : workers_) {
        if (thread.joinable()) { thread.join(); }
    }
    startSignal_.clear(std::memory_order_release);
    queueActive_ = false;
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::initQueue() {
    if (queueActive_) {
        std::cerr << "SpapQueue is already active and cannot be initiated again!";
    } else {
        queueActive_ = true;

        [this]<std::size_t... I>(std::index_sequence<I...>) {
            ((workers_[I] = std::jthread(std::bind_front(&ThisQType::threadWork<I>, this))), ...);
        }(std::make_index_sequence<netw.numWorkers_>{});

        allocateSignal_.arrive_and_wait();
    }
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::processQueue() {
    startSignal_.test_and_set(std::memory_order_release);
    startSignal_.notify_all();
};

// template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename
// LocalQType> template <std::size_t tupleSize,
//           bool networkHomogeneousInPorts,
//           std::enable_if_t<not networkHomogeneousInPorts, bool>>
// inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternalHelper(const T &val,
//                                                                                const std::size_t workerId,
//                                                                                const std::size_t port) {
//     static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
//     if constexpr (tupleSize == netw.numWorkers_) { assert(workerId < netw.numWorkers_); }

//     if (workerId == netw.numWorkers_ - tupleSize) {
//         return std::get<netw.numWorkers_ - tupleSize>(workerResources_).push(val, port);
//     } else {
//         if constexpr (tupleSize > 1) {
//             return pushInternalHelper<tupleSize - 1>(val, workerId, port);
//         } else {
// #ifdef __cpp_lib_unreachable
//             std::unreachable();
// #endif
//             assert(false);
//             return false;
//         }
//     }
// }

// template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename
// LocalQType> template <std::size_t tupleSize,
//           bool networkHomogeneousInPorts,
//           std::enable_if_t<not networkHomogeneousInPorts, bool>>
// inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternalHelper(T &&val,
//                                                                                const std::size_t workerId,
//                                                                                const std::size_t port) {
//     static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
//     if constexpr (tupleSize == netw.numWorkers_) { assert(workerId < netw.numWorkers_); }

//     if (workerId == (netw.numWorkers_ - tupleSize)) {
//         return std::get<netw.numWorkers_ - tupleSize>(workerResources_).push(std::move(val), port);
//     } else {
//         if constexpr (tupleSize > 1) {
//             return pushInternalHelper<tupleSize - 1>(std::move(val), workerId, port);
//         } else {
// #ifdef __cpp_lib_unreachable
//             std::unreachable();
// #endif
//             assert(false);
//             return false;
//         }
//     }
// }

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
        return std::get<netw.numWorkers_ - tupleSize>(workerResources_)->push(first, last, port);
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

// template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename
// LocalQType> inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternal(const T &val,
//                                                                          const std::size_t workerId,
//                                                                          const std::size_t port) {
//     if constexpr (netw.hasHomogeneousInPorts()) {
//         return workerResources_[workerId]->push(val, port);
//     } else {
//         return pushInternalHelper<netw.numWorkers_>(val, workerId, port);
//     }
// }

// template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename
// LocalQType> inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternal(T &&val,
//                                                                          const std::size_t workerId,
//                                                                          const std::size_t port) {
//     if constexpr (netw.hasHomogeneousInPorts()) {
//         return workerResources_[workerId]->push(std::move(val), port);
//     } else {
//         return pushInternalHelper<netw.numWorkers_>(std::move(val), workerId, port);
//     }
// }

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
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::threadWork(std::stop_token stoken) {
    static_assert(N < netw.numWorkers_);

    // pinning thread
    pthread_t self = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    const std::size_t logicalCore = netw.logicalCore_[N];
    CPU_SET(logicalCore, &cpuset);

    int rc = pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        const std::string errorMessage = "Call to pthread_setaffinity_np returned error "
                                         + std::to_string(rc)
                                         + ".\nFailed to pin worker thread number "
                                         + std::to_string(N)
                                         + " to logical core "
                                         + std::to_string(logicalCore)
                                         + ".\n";
        std::cerr << errorMessage;
        std::exit(EXIT_FAILURE);
    }

    // init resource
    WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[N]> resource
        = WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[N]>(*this, tables::qNetworkTable<netw, N>());

    // set reference
    if constexpr (netw.hasHomogeneousInPorts()) {
        workerResources_[N] = &resource;
    } else {
        std::get<N>(workerResources_) = &resource;
    }

    // signal reference set
#ifdef SPAPQ_DEBUG
    std::cout << "Worker "
                     + std::to_string(N)
                     + " built local queue and waits until all allocations have been made.\n";
#endif
    allocateSignal_.arrive_and_wait();

    // awaiting unsafe enqueuing and global starting signal
#ifdef SPAPQ_DEBUG
    std::cout << "Worker " + std::to_string(N) + " is waiting for starting signal.\n";
#endif
    startSignal_.wait(false, std::memory_order_acquire);

    // run
#ifdef SPAPQ_DEBUG
    std::cout << "Worker " + std::to_string(N) + " begins running the queue.\n";
#endif
    resource.run(stoken);

    // signal and await process finished
#ifdef SPAPQ_DEBUG
    std::cout << "Worker " + std::to_string(N) + " has finished and waits for other workers.\n";
#endif
    safeToDeallocateSignal_.arrive_and_wait();

    // unset reference
    if constexpr (netw.hasHomogeneousInPorts()) {
        workerResources_[N] = nullptr;
    } else {
        std::get<N>(workerResources_) = nullptr;
    }
#ifdef SPAPQ_DEBUG
    std::cout << "Worker " + std::to_string(N) + " deleted reference to local queue.\n";
#endif
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
