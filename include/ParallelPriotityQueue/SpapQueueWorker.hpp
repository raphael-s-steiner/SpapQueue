#pragma once

#include <iterator>
#include <stop_token>

#include "Discrepancy/QNetworkTables.hpp"
#include "Discrepancy/TableGenerator.hpp"
#include "RingBuffer/RingBuffer.hpp"

namespace spapq {

// TODO: concept for localQType

/**
 * @brief A base class for the functionality of the local worker of the (global) sparse parallel approximate
 * priority queue (SpapQueue).
 *
 * @tparam GlobalQType Type of the global queue which employs/deploys this worker.
 * @tparam LocalQType Type of the local (worker personal) queue.
 * @tparam numPorts The number of ports or incomming channels to the worker.
 *
 * @see SpapQueue
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
class WorkerResource {
    template <typename, typename, std::size_t>
    friend class WorkerResource;
    template <typename, QNetwork, template <class, class, std::size_t> class, typename>
    friend class SpapQueue;

  public:
    using value_type = GlobalQType::value_type;

  private:
    const std::array<std::size_t, tables::maxTableSize<GlobalQType::netw_>()>
        channelIndices_;                                                         ///< Order of outgoing
                                                                                 ///< channels to push to.
    std::array<value_type, GlobalQType::netw_.maxBatchSize()> outBuffer_;        ///< Small buffer before
                                                                                 ///< pushing to outgoing
                                                                                 ///< channel.

    const std::size_t workerId_;        ///< Worker Id in the global queue.
    std::size_t localCount_{0U};        ///< A partial account of the number of tasks in the global queue.
    GlobalQType &globalQueue_;          ///< Reference to the global queue.
    typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator
        bufferPointer_;        ///< Pointer to the next free spot in the outBuffer_.
    typename std::array<std::size_t, tables::maxTableSize<GlobalQType::netw_>()>::const_iterator
        channelPointer_;        ///< Pointer to the next outgoing channel.
    const typename std::array<std::size_t, tables::maxTableSize<GlobalQType::netw_>()>::const_iterator
        channelTableEndPointer_;        ///< Pointer to the end of the channel indices table. Used to unify
                                        ///< the worker type.

    LocalQType queue_;        ///< Worker local queue.
    std::array<RingBuffer<value_type, GlobalQType::netw_.channelBufferSize_>, numPorts>
        inPorts_;        ///< Incomming channels.

    inline void incrGlobalCount() noexcept;
    inline void decrGlobalCount() noexcept;

    [[nodiscard("Push may fail when channel is full.\n")]] inline bool pushOutBuffer() noexcept;
    inline void pushOutBufferSelf(
        const typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator fromPointer) noexcept;

    inline void enqueueInChannels() noexcept;
    virtual void processElement(const value_type val) noexcept = 0;

    [[nodiscard("Push may fail when channel is full.\n")]] inline bool push(const value_type val,
                                                                            std::size_t port) noexcept;
    template <class InputIt>
    [[nodiscard("Push may fail when channel is full.\n")]] inline bool push(InputIt first,
                                                                            InputIt last,
                                                                            std::size_t port) noexcept;

    inline void pushUnsafe(const value_type val) noexcept;

    inline void run(std::stop_token stoken) noexcept;

  protected:
    inline std::size_t workerId() const noexcept;
    inline void enqueueGlobal(const value_type val) noexcept;

    template <std::size_t channelIndicesLength, typename... Args>
    constexpr WorkerResource(GlobalQType &globalQueue,
                             const std::array<std::size_t, channelIndicesLength> &channelIndices,
                             std::size_t workerId,
                             Args &&...localQargs);

  public:
    WorkerResource(const WorkerResource &other) = delete;
    WorkerResource(WorkerResource &&other) = delete;
    WorkerResource &operator=(const WorkerResource &other) = delete;
    WorkerResource &operator=(WorkerResource &&other) = delete;
    virtual ~WorkerResource() = default;
};

/**
 * @brief Check whether the worker template of the SpapQueue is derived from the base template WorkerResource.
 *
 * @tparam WorkerTemplate Derived worker class.
 * @tparam GlobalQType Global queue type.
 * @tparam LocalQType Worker local queue type.
 * @tparam N First N workers are checked.
 *
 * @see SpapQueue
 * @see WorkerResource
 */
template <template <class, class, std::size_t> class WorkerTemplate, class GlobalQType, class LocalQType, std::size_t N>
constexpr bool isDerivedWorkerResource() {
    static_assert(N <= GlobalQType::netw_.numWorkers_);

    if constexpr (N == 0U) {
        return true;
    } else {
        constexpr bool val
            = std::is_base_of<WorkerResource<GlobalQType, LocalQType, GlobalQType::netw_.numPorts_[N - 1]>,
                              WorkerTemplate<GlobalQType, LocalQType, GlobalQType::netw_.numPorts_[N - 1]>>::value;
        return val && isDerivedWorkerResource<WorkerTemplate, GlobalQType, LocalQType, N - 1>;
    }
}

/**
 * @brief Helper class to create a tuple of workers.
 *
 * @tparam WorkerTemplate Derived worker class.
 * @tparam GlobalQType Global queue type.
 * @tparam LocalQType Worker local queue type.
 * @tparam N Tuple of first N workers.
 */
template <template <class, class, std::size_t> class WorkerTemplate, class GlobalQType, class LocalQType, std::size_t N>
struct WorkerCollectiveHelper {
    static_assert(N <= GlobalQType::netw_.numWorkers_);
    template <typename... Args>
    using type = typename WorkerCollectiveHelper<WorkerTemplate, GlobalQType, LocalQType, N - 1>::
        template type<WorkerTemplate<GlobalQType, LocalQType, GlobalQType::netw_.numPorts_[N - 1]> *, Args...>;
};

template <template <class, class, std::size_t> class WorkerTemplate, class GlobalQType, class LocalQType>
struct WorkerCollectiveHelper<WorkerTemplate, GlobalQType, LocalQType, 0> {
    template <typename... Args>
    using type = std::tuple<Args...>;
};

// Implementation details

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
template <std::size_t channelIndicesLength, typename... Args>
constexpr WorkerResource<GlobalQType, LocalQType, numPorts>::WorkerResource(
    GlobalQType &globalQueue,
    const std::array<std::size_t, channelIndicesLength> &channelIndices,
    std::size_t workerId,
    Args &&...localQargs) :
    channelIndices_(
        tables::extendTable<tables::maxTableSize<GlobalQType::netw_>(), channelIndicesLength>(channelIndices)),
    workerId_(workerId),
    globalQueue_(globalQueue),
    bufferPointer_(outBuffer_.begin()),
    channelPointer_(channelIndices_.cbegin()),
    channelTableEndPointer_(std::next(channelIndices_.cbegin(), channelIndicesLength)),
    queue_(std::forward<Args>(localQargs)...) { }

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::push(const value_type val,
                                                                    std::size_t port) noexcept {
    return inPorts_[port].push(val);
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
template <class InputIt>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::push(InputIt first,
                                                                    InputIt last,
                                                                    std::size_t port) noexcept {
    return inPorts_[port].push(first, last);
}

/**
 * @brief Adds a new task to the global queue.
 *
 * @param val Task.
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::enqueueGlobal(const value_type val) noexcept {
    assert(bufferPointer_ != outBuffer_.end());

    incrGlobalCount();
    *bufferPointer_ = val;
    ++bufferPointer_;

    std::size_t maxAttempts = GlobalQType::netw_.maxPushAttempts_;
    while (static_cast<std::size_t>(std::distance(outBuffer_.begin(), bufferPointer_))
               >= GlobalQType::netw_.batchSize_[*channelPointer_]
           && maxAttempts > 0U) {
        if (not pushOutBuffer()) { --maxAttempts; }

        ++channelPointer_;
        if (channelPointer_ == channelTableEndPointer_) { channelPointer_ = channelIndices_.cbegin(); }
    }
    if (maxAttempts == 0U) [[unlikely]] { pushOutBufferSelf(outBuffer_.begin()); }
}

/**
 * @brief Pushes the outbuffer to the current outgoing channel.
 *
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::pushOutBuffer() noexcept {
    bool successfulPush = false;

    const std::size_t batch = GlobalQType::netw_.batchSize_[*channelPointer_];
    assert(batch <= static_cast<std::size_t>(std::distance(outBuffer_.begin(), bufferPointer_)));
    const typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator itBegin = std::prev(
        bufferPointer_,
        static_cast<typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::difference_type>(
            batch));

    const std::size_t targetWorker = GlobalQType::netw_.edgeTargets_[*channelPointer_];
    if (targetWorker == GlobalQType::netw_.numWorkers_) {        // netw.numWorkers_ is reserved for self-push
        pushOutBufferSelf(itBegin);
        successfulPush = true;
    } else {
        const std::size_t port = GlobalQType::netw_.targetPort_[*channelPointer_];
        successfulPush = globalQueue_.pushInternal(itBegin, bufferPointer_, targetWorker, port);
        if (successfulPush) { bufferPointer_ = itBegin; }
    }

    return successfulPush;
}

/**
 * @brief Pushes all task from (including) fromPointer in the outbuffer to the local queue.
 *
 * @param fromPointer
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::pushOutBufferSelf(
    const typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator fromPointer) noexcept {
    constexpr bool hasBatchPush
        = requires (LocalQType &q,
                    typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator first,
                    typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator last) {
              q.push(first, last);
          };

    if constexpr (hasBatchPush) {
        auto it = fromPointer;
        queue_.push(it, bufferPointer_);
    } else {
        for (auto it = fromPointer; it != bufferPointer_; ++it) { queue_.push(*it); }
    }
    bufferPointer_ = fromPointer;
}

/**
 * @brief Enqueues all tasks in the incomming channels into the local queue.
 *
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::enqueueInChannels() noexcept {
    for (auto &portRingBuffer : inPorts_) {
        std::optional<value_type> data = portRingBuffer.pop();
        while (data.has_value()) {
            queue_.push(*data);
            data = portRingBuffer.pop();
        }
    }
}

/**
 * @brief Starts running the local worker and processes the queue until the global queue is empty or stop has
 * been requested via stop token.
 *
 * @param stoken Stop token.
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::run(std::stop_token stoken) noexcept {
    std::size_t cntr = 0;
    while (globalQueue_.globalCount_.load(std::memory_order_acquire) > 0 && (not stoken.stop_requested())) {
        while ((not queue_.empty())) [[likely]] {
            if (cntr % 128U == 0U) {
                if (stoken.stop_requested()) [[unlikely]] { break; }
            }

            if (cntr % GlobalQType::netw_.enqueueFrequency_ == 0U) { enqueueInChannels(); }

            const value_type val = queue_.top();
            queue_.pop();
            processElement(val);
            decrGlobalCount();

            ++cntr;
        }
        enqueueInChannels();
        pushOutBufferSelf(outBuffer_.begin());
    }
}

/**
 * @brief Pushes a task directly into the local queue. This should never be called when the worker is
 * running/processing the global queue.
 *
 * @param val Task.
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::pushUnsafe(const value_type val) noexcept {
    queue_.push(val);
}

/**
 * @brief Returns the worker Id in the global queue.
 *
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline std::size_t WorkerResource<GlobalQType, LocalQType, numPorts>::workerId() const noexcept {
    return workerId_;
}

/**
 * @brief Increases the global count by one. Recall the global count is split between globalCount_ in the
 * global queue and localCount_ in all local queues.
 *
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::incrGlobalCount() noexcept {
    ++localCount_;
    const std::size_t qSize = queue_.size();
    if (localCount_ >= qSize) {
        const std::size_t newLocalCount = qSize / 2;
        const std::size_t diff = localCount_ - newLocalCount;

        localCount_ = newLocalCount;
        globalQueue_.globalCount_.fetch_add(diff, std::memory_order_relaxed);
    }
}

/**
 * @brief Decreases the global count by one. Recall the global count is split between globalCount_ in the
 * global queue and localCount_ in all local queues.
 *
 */
template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::decrGlobalCount() noexcept {
    if (localCount_ == 0) {
        const std::size_t newLocalCount = queue_.size() / 2;
        const std::size_t diff = newLocalCount + 1;

        localCount_ = newLocalCount;
        globalQueue_.globalCount_.fetch_sub(diff, std::memory_order_relaxed);
    } else {
        --localCount_;
    }
}

}        // end namespace spapq
