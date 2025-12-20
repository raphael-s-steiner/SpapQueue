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
class SpapQueue final {
    template <typename, typename, std::size_t>
    friend class WorkerTemplate;
    template <typename, typename, std::size_t>
    friend class WorkerResource;

  public:
    using value_type = T;
    static constexpr QNetwork<netw.numWorkers_, netw.numChannels_> netw_{netw};

    template <typename... Args>
    bool initQueue(Args &&...workerArgs);
    void processQueue();
    void waitProcessFinish();
    void requestStop();

    inline void pushUnsafe(const value_type val, const std::size_t workerId = 0U) noexcept;

    // TODO pushSafe

    SpapQueue() = default;
    SpapQueue(const SpapQueue &other) = delete;
    SpapQueue(SpapQueue &&other) = delete;
    SpapQueue &operator=(const SpapQueue &other) = delete;
    SpapQueue &operator=(SpapQueue &&other) = delete;
    ~SpapQueue() noexcept;

  private:
    using ThisQType = SpapQueue<T, netw, WorkerTemplate, LocalQType>;

    using WorkerCollective = std::conditional_t<
        netw.hasHomogeneousInPorts(),
        std::array<WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[0U]> *, netw.numWorkers_>,
        typename WorkerCollectiveHelper<WorkerTemplate, ThisQType, LocalQType, netw, netw.numWorkers_>::template type<>
    >;

    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> globalCount_{0U};
    alignas(CACHE_LINE_SIZE) WorkerCollective workerResources_;

    std::atomic<bool> queueActive_{false};
    std::atomic_flag startSignal_;
    std::barrier<> allocateSignal_{netw.numWorkers_ + 1};
    std::barrier<> safeToDeallocateSignal_{netw.numWorkers_};

    std::array<std::jthread, netw.numWorkers_> workers_;

    template <std::size_t N, typename... Args>
    void threadWork(std::stop_token stoken, Args &&...workerArgs);

    template <class InputIt>
    [[nodiscard("Push may fail when queue is full.\n")]] inline bool pushInternal(
        InputIt first, InputIt last, const std::size_t workerId, const std::size_t port) noexcept;

    // Helper functions
    template <std::size_t tupleSize,
              class InputIt,
              bool networkHomogeneousInPorts = netw.hasHomogeneousInPorts(),
              std::enable_if_t<not networkHomogeneousInPorts, bool> = true>
    [[nodiscard("Push may fail when queue is full.\n")]] inline bool pushInternalHelper(
        InputIt first, InputIt last, const std::size_t workerId, const std::size_t port) noexcept;

    template <std::size_t tupleSize,
              bool networkHomogeneousInPorts = netw.hasHomogeneousInPorts(),
              std::enable_if_t<not networkHomogeneousInPorts, bool> = true>
    inline void pushUnsafeHelper(const value_type val, const std::size_t workerId) noexcept;

    // Static asserts
    static_assert(netw.isValidQNetwork(), "The QNetwork needs to be valid!\n");
    static_assert(std::is_same_v<value_type, typename LocalQType::value_type>,
                  "The local queue type needs to have matching value_type!\n");
    static_assert(isDerivedWorkerResource<WorkerTemplate, ThisQType, LocalQType, netw.numWorkers_>(),
                  "WorkerTemplate must be derived from WorkerResource.\n");
    static_assert(netw.hasSeparateLogicalCores(), "Workers should be on separate logical Cores.\n");
};

// Implementation Details

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::waitProcessFinish() {
    for (auto &thread : workers_) {
        if (thread.joinable()) { thread.join(); }
    }
    globalCount_.store(0U, std::memory_order_relaxed);    // In case a stop was requested
    startSignal_.clear(std::memory_order_relaxed);
    queueActive_.store(false, std::memory_order_release);
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <typename... Args>
bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::initQueue(Args &&...workerArgs) {
    if (queueActive_.exchange(true, std::memory_order_acq_rel)) {
        std::cerr << "SpapQueue is already active and cannot be initiated again!\n";
        return false;
    }

    [this, &workerArgs...]<std::size_t... I>(std::index_sequence<I...>) {
        ((workers_[I] = std::jthread(std::bind_front(&ThisQType::threadWork<I, Args...>, this),
                                     std::forward<Args>(workerArgs)...)),
         ...);
    }(std::make_index_sequence<netw.numWorkers_>{});

    allocateSignal_.arrive_and_wait();
    return true;
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::processQueue() {
    startSignal_.test_and_set(std::memory_order_release);
    startSignal_.notify_all();
};

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t tupleSize,
          class InputIt,
          bool networkHomogeneousInPorts,
          std::enable_if_t<not networkHomogeneousInPorts, bool>>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternalHelper(
    InputIt first, InputIt last, const std::size_t workerId, const std::size_t port) noexcept {
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
#else
            assert(false);
#endif
            return false;
        }
    }
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <class InputIt>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushInternal(InputIt first,
                                                                         InputIt last,
                                                                         const std::size_t workerId,
                                                                         const std::size_t port) noexcept {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(first, last, port);
    } else {
        return pushInternalHelper<netw.numWorkers_, InputIt>(first, last, workerId, port);
    }
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t N, typename... Args>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::threadWork(std::stop_token stoken, Args &&...workerArgs) {
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
                                         + ".\nFailed to pin worker number "
                                         + std::to_string(N)
                                         + "'s thread to logical core "
                                         + std::to_string(logicalCore)
                                         + ".\n";
        std::cerr << errorMessage;
        std::exit(EXIT_FAILURE);
    }

#ifdef SPAPQ_DEBUG
    std::cout << "Worker "
                     + std::to_string(N)
                     + " spawned on core "
                     + std::to_string(sched_getcpu())
                     + " with thread "
                     + std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id()))
                     + "\n";
#endif

    // init resource
    WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[N]> resource
        = WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[N]>(
            *this, tables::qNetworkTable<netw, N>(), N, std::forward<Args>(workerArgs)...);

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

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::requestStop() {
    if (not queueActive_.load(std::memory_order_acquire)) { return; }

    for (auto &workerThread : workers_) { workerThread.request_stop(); }
    processQueue();    // In case worker threads are waiting for start signal
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
SpapQueue<T, netw, WorkerTemplate, LocalQType>::~SpapQueue() noexcept {
    queueActive_.store(true, std::memory_order_relaxed);    // Such that nobody else can start the queue
    requestStop();    // Required because worker threads can be stuck awaiting start signal
    // Deconstructor of jthread automatically joins the worker threads and thus destroys the worker resources
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t tupleSize,
          bool networkHomogeneousInPorts,
          std::enable_if_t<not networkHomogeneousInPorts, bool>>
inline void SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushUnsafeHelper(
    const value_type val, const std::size_t workerId) noexcept {
    static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
    if constexpr (tupleSize == netw.numWorkers_) { assert(workerId < netw.numWorkers_); }

    if (workerId == (netw.numWorkers_ - tupleSize)) {
        return std::get<netw.numWorkers_ - tupleSize>(workerResources_)->pushUnsafe(val);
    } else {
        if constexpr (tupleSize > 1) { pushUnsafeHelper<tupleSize - 1>(val, workerId); }
    }
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
inline void SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushUnsafe(const value_type val,
                                                                       const std::size_t workerId) noexcept {
    if constexpr (netw.hasHomogeneousInPorts()) {
        workerResources_[workerId]->pushUnsafe(val);
    } else {
        pushUnsafeHelper<netw.numWorkers_>(val, workerId);
    }
    globalCount_.fetch_add(1U, std::memory_order_release);
}

}    // end namespace spapq
