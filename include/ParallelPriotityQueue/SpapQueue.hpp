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

/**
 * @brief SpapQueue is a lock-free parallel approximate priority queue. To run the queue call\n
 * (1) initQueue, which allocates the workers,\n
 * (2) pushBeforeProcessing, to populate the queue with initial tasks,\n
 * (3) processQueue, to let the workers start processing the queue,\n
 * (4) pushDuringProcessing, whilst the queue is running (and only then) additional tasks may be enqueued on
 *     self-push channels,\n
 * (5) waitProcessFinish, to wait till all of the tasks in the queue have been completed.
 *
 * Once the queue has completed, it can be reused following the same steps. Calling the functions in any other
 * order results in undefined behaviour.
 *
 * The queue may be interrupted at any point (by the main thread operating on the queue) by calling
 * requestStop.
 *
 * The SpapQueue class or object itself is generally not considered thread-safe with a few exceptions.\n
 * (a) pushBeforeProcessing can be called for each worker by at most one thread. Hence, netw.numWorker_ number
 *     of threads can populate the queue.\n
 * (b) pushDuringProcessing can be called for each (self-push) channel by at most one thread.
 *
 * @tparam T Type of queue element or task.
 * @tparam netw QNetwork which dictates the linking of the workers and other queue related information.
 * @tparam WorkerTemplate The worker type to be used for the queue. Needs to inherit from WorkerResource.
 * @tparam LocalQType Type of the local queue of each individual worker.
 *
 * @see QNetwork
 * @see WorkerResource
 */
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

    inline void pushBeforeProcessing(const value_type val, const std::size_t workerId = 0U) noexcept;
    template <std::size_t channel>
    [[nodiscard("Push may fail when channel is full or queue has already finished.\n")]] inline bool
    pushDuringProcessing(const value_type val) noexcept;

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
        typename WorkerCollectiveHelper<WorkerTemplate, ThisQType, LocalQType, netw.numWorkers_>::template type<>
    >;

    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> globalCount_{
        0U};        ///< Is zero if and only if there is no task in the queue. Together with all the
                    ///< localCount_ of workers keeps track of the total number of tasks in the global queue.
    alignas(CACHE_LINE_SIZE) WorkerCollective workerResources_;        ///< Resources of the workers.

    std::atomic<bool> queueActive_{false};        ///< Keeps track whether the queue is active, i.e., whether
                                                  ///< worker threads have been spawned.
    std::atomic_flag startSignal_;        ///< The signal for the workers to start working on the tasks in the
                                          ///< queue.
    std::barrier<> allocateSignal_{netw.numWorkers_ + 1};        ///< Signals that it is now safe to enqueue
                                                                 ///< tasks.
    std::barrier<> safeToDeallocateSignal_{netw.numWorkers_};        ///< Signal that all workers have finished
                                                                     ///< working and that it is now safe to
                                                                     ///< deallocate the worker resources.

    std::array<std::jthread, netw.numWorkers_> workers_;        ///< Worker threads.

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
    inline void pushBeforeProcessingHelper(const value_type val, const std::size_t workerId) noexcept;

    // Static asserts
    static_assert(netw.isValidQNetwork(), "The QNetwork needs to be valid!\n");
    static_assert(std::is_same_v<value_type, typename LocalQType::value_type>,
                  "The local queue type needs to have matching value_type!\n");
    static_assert(isDerivedWorkerResource<WorkerTemplate, ThisQType, LocalQType, netw.numWorkers_>(),
                  "WorkerTemplate must be derived from WorkerResource.\n");
    static_assert(netw.hasSeparateLogicalCores(), "Workers should be on separate logical Cores.\n");
    static_assert(netw.isStronglyConnected(), "Required to keep all workers busy.\n");
    static_assert(std::is_nothrow_default_constructible_v<value_type>);
    static_assert(std::is_nothrow_copy_constructible_v<value_type>);
};

// Implementation Details

/**
 * @brief Wait till the whole queue has finished processing all tasks.
 *
 */
template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::waitProcessFinish() {
    for (auto &thread : workers_) {
        if (thread.joinable()) { thread.join(); }
    }
    globalCount_.store(0U, std::memory_order_relaxed);        // In case a stop was requested
    startSignal_.clear(std::memory_order_relaxed);
    queueActive_.store(false, std::memory_order_release);
}

/**
 * @brief Initialises the queue by allocating resources and the worker thread.
 *
 * @param workerArgs Arguments to be forwarded to the workers. Note that arguments passed by reference need to
 * be wrapped by std::ref.
 * @return true If initialisation has succeeded, i.e., not already initialised.
 * @return false If the queue is already active.
 */
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

/**
 * @brief Signals the workers to begin processing the queue.
 *
 */
template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::processQueue() {
    startSignal_.test_and_set(std::memory_order_release);
    startSignal_.notify_all();
}

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

/**
 * @brief Batch push onto channel, return whether succeeded.
 *
 */
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

/**
 * @brief Intructions to be executed by the worker.
 *
 * @param stoken Stop token
 * @param workerArgs Arguments to be passed to the worker constructor.
 */
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

/**
 * @brief Request early stop or termination of the queue.
 *
 */
template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
void SpapQueue<T, netw, WorkerTemplate, LocalQType>::requestStop() {
    if (not queueActive_.load(std::memory_order_acquire)) { return; }

    for (auto &workerThread : workers_) { workerThread.request_stop(); }
    processQueue();        // In case worker threads are waiting for start signal
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
SpapQueue<T, netw, WorkerTemplate, LocalQType>::~SpapQueue() noexcept {
    queueActive_.store(true, std::memory_order_relaxed);        // Such that nobody else can start the queue
    requestStop();        // Required because worker threads can be stuck awaiting start signal
    // Deconstructor of jthread automatically joins the worker threads and thus destroys the worker resources
}

template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t tupleSize,
          bool networkHomogeneousInPorts,
          std::enable_if_t<not networkHomogeneousInPorts, bool>>
inline void SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushBeforeProcessingHelper(
    const value_type val, const std::size_t workerId) noexcept {
    static_assert(0 < tupleSize && tupleSize <= netw.numWorkers_);
    if constexpr (tupleSize == netw.numWorkers_) { assert(workerId < netw.numWorkers_); }

    if (workerId == (netw.numWorkers_ - tupleSize)) {
        return std::get<netw.numWorkers_ - tupleSize>(workerResources_)->pushUnsafe(val);
    } else {
        if constexpr (tupleSize > 1) { pushBeforeProcessingHelper<tupleSize - 1>(val, workerId); }
    }
}

/**
 * @brief Enqueues initial tasks into the local queue of a worker. Only to be used after initialisation and
 * before processing the queue.
 *
 * @param val Task or queue element.
 * @param workerId Worker id whose local queue to push to.
 */
template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
inline void SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushBeforeProcessing(
    const value_type val, const std::size_t workerId) noexcept {
    if constexpr (netw.hasHomogeneousInPorts()) {
        workerResources_[workerId]->pushUnsafe(val);
    } else {
        pushBeforeProcessingHelper<netw.numWorkers_>(val, workerId);
    }
    globalCount_.fetch_add(1U, std::memory_order_release);
}

/**
 * @brief Enqueues tasks into a self-push channel of the queue. Only to be used after initialisation and
 * during processing the queue.
 *
 * @tparam channel A self-push channel into which to push.
 * @param val Task or queue element.
 * @return true If push succeeded.
 * @return false If push failed. This is either because the channel buffer is full or the queue has already
 * finished.
 */
template <typename T, QNetwork netw, template <class, class, std::size_t> class WorkerTemplate, typename LocalQType>
template <std::size_t channel>
inline bool SpapQueue<T, netw, WorkerTemplate, LocalQType>::pushDuringProcessing(const value_type val) noexcept {
    static_assert(channel < netw.numChannels_, "Must be a valid channel in the QNetwork.");
    static_assert(netw.edgeTargets_[channel] == netw.numWorkers_, "Channel must not have a producer.");

    bool success = false;

    // Checks if queue is still running and if so signals that there is more work to come
    std::size_t prevCount = globalCount_.load(std::memory_order_relaxed);
    while (prevCount > 0U
           && (not globalCount_.compare_exchange_weak(
               prevCount, prevCount + 1U, std::memory_order_relaxed, std::memory_order_relaxed))) { };

    // Only inserts if queue is still running
    if (prevCount > 0U) {
        constexpr std::size_t worker = netw.source(channel);
        constexpr std::size_t port = netw.targetPort_[channel];

        if constexpr (netw.hasHomogeneousInPorts()) {
            success = workerResources_[worker]->push(val, port);
        } else {
            success = std::get<worker>(workerResources_)->push(val, port);
        }

        if (not success) { globalCount_.fetch_sub(1U, std::memory_order_relaxed); }
    }

    return success;
}

}        // end namespace spapq
