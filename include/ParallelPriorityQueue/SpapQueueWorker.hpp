/*
Copyright 2025 Raphael S. Steiner

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

@author Raphael S. Steiner
*/

#pragma once

#include <iterator>
#include <stop_token>

#include "Discrepancy/QNetworkTables.hpp"
#include "Discrepancy/TableGenerator.hpp"
#include "ParallelPriorityQueue/Concepts/BasicQueue.hpp"
#include "RingBuffer/RingBuffer.hpp"

namespace spapq {

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
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
class WorkerResource {
    template <typename, BasicQueue, std::size_t>
    friend class WorkerResource;
    template <typename, QNetwork, template <typename, BasicQueue, std::size_t> class, BasicQueue>
    friend class SpapQueue;

  public:
    using value_type = GlobalQType::value_type;

  private:
    const std::array<std::size_t, tables::maxTableSize<GlobalQType::netw_>()>
        channelIndices_;        ///< Order of outgoing
                                ///< channels to push to.
    std::array<value_type, 2U * GlobalQType::netw_.maxBatchSize()> outBuffer_;        ///< Small buffer before
                                                                                      ///< pushing to outgoing
                                                                                      ///< channel.

    const std::size_t workerId_;        ///< Worker Id in the global queue.
    std::size_t localCount_{0U};        ///< A partial account of the number of tasks in the global queue.
    GlobalQType &globalQueue_;          ///< Reference to the global queue.
    std::size_t bufferHead_{0U};        ///< Head of out ring buffer.
    std::size_t bufferTail_{0U};        ///< Tail of out ring buffer.
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
    inline void pushOutBufferSelf(const std::size_t numElements) noexcept;

    inline void enqueueInChannels() noexcept;
    virtual void processElement(const value_type val) noexcept = 0;

    [[nodiscard("Push may fail when channel is full.\n")]] inline bool push(const value_type val,
                                                                            const std::size_t port) noexcept;
    template <class InputIt>
    [[nodiscard("Push may fail when channel is full.\n")]] inline bool push(InputIt first,
                                                                            InputIt last,
                                                                            const std::size_t port) noexcept;

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
template <template <typename, BasicQueue, std::size_t> class WorkerTemplate,
          typename GlobalQType,
          BasicQueue LocalQType,
          std::size_t N>
consteval bool isDerivedWorkerResource() {
    static_assert(N <= GlobalQType::netw_.numWorkers_);

    if constexpr (N == 0U) {
        return true;
    } else {
        constexpr bool val
            = std::is_base_of<WorkerResource<GlobalQType, LocalQType, GlobalQType::netw_.numPorts_[N - 1]>,
                              WorkerTemplate<GlobalQType, LocalQType, GlobalQType::netw_.numPorts_[N - 1]>>::value;
        return val && isDerivedWorkerResource<WorkerTemplate, GlobalQType, LocalQType, N - 1>();
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
template <template <typename, BasicQueue, std::size_t> class WorkerTemplate,
          typename GlobalQType,
          BasicQueue LocalQType,
          std::size_t N>
struct WorkerCollectiveHelper {
    static_assert(N <= GlobalQType::netw_.numWorkers_);
    template <typename... Args>
    using type = typename WorkerCollectiveHelper<WorkerTemplate, GlobalQType, LocalQType, N - 1>::
        template type<WorkerTemplate<GlobalQType, LocalQType, GlobalQType::netw_.numPorts_[N - 1]> *, Args...>;
};

template <template <typename, BasicQueue, std::size_t> class WorkerTemplate, typename GlobalQType, BasicQueue LocalQType>
struct WorkerCollectiveHelper<WorkerTemplate, GlobalQType, LocalQType, 0> {
    template <typename... Args>
    using type = std::tuple<Args...>;
};

// Implementation details

template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
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
    channelPointer_(channelIndices_.cbegin()),
    channelTableEndPointer_(std::next(channelIndices_.cbegin(), channelIndicesLength)),
    queue_(std::forward<Args>(localQargs)...) { }

template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::push(const value_type val,
                                                                    const std::size_t port) noexcept {
    return inPorts_[port].push(val);
}

template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
template <class InputIt>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::push(InputIt first,
                                                                    InputIt last,
                                                                    const std::size_t port) noexcept {
    return inPorts_[port].push(first, last);
}

/**
 * @brief Adds a new task to the global queue.
 *
 * @param val Task.
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::enqueueGlobal(const value_type val) noexcept {
    assert(bufferTail_ <= bufferHead_);
    assert(bufferHead_ < bufferTail_ + outBuffer_.size());

    incrGlobalCount();
    outBuffer_[bufferHead_ % outBuffer_.size()] = val;
    ++bufferHead_;

    std::size_t maxAttempts = GlobalQType::netw_.maxPushAttempts_;
    while (bufferHead_ - bufferTail_ >= GlobalQType::netw_.batchSize_[*channelPointer_] && maxAttempts > 0U) {
        if (not pushOutBuffer()) { --maxAttempts; }

        ++channelPointer_;
        if (channelPointer_ == channelTableEndPointer_) { channelPointer_ = channelIndices_.cbegin(); }
    }
    if (maxAttempts == 0U) [[unlikely]] { pushOutBufferSelf(bufferHead_ - bufferTail_); }
}

/**
 * @brief Pushes the outbuffer to the current outgoing channel.
 *
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::pushOutBuffer() noexcept {
    const std::size_t batch = GlobalQType::netw_.batchSize_[*channelPointer_];
    assert(batch <= bufferHead_ - bufferTail_);

    const std::size_t targetWorker = GlobalQType::netw_.edgeTargets_[*channelPointer_];
    if (targetWorker == GlobalQType::netw_.numWorkers_) {        // netw.numWorkers_ is reserved for self-push
        pushOutBufferSelf(batch);
        return true;
    } else {
        const std::size_t reducedTail = bufferTail_ % outBuffer_.size();
        const std::size_t numElementsFirstPush = std::min(outBuffer_.size() - reducedTail, batch);
        const std::size_t numElementsSecondPush = batch - numElementsFirstPush;

        const auto itBeginFirst = std::next(
            outBuffer_.begin(), static_cast<typename decltype(outBuffer_)::difference_type>(reducedTail));
        const auto itEndFirst = std::next(
            itBeginFirst, static_cast<typename decltype(outBuffer_)::difference_type>(numElementsFirstPush));
        const auto itEndSecond
            = std::next(outBuffer_.begin(),
                        static_cast<typename decltype(outBuffer_)::difference_type>(numElementsSecondPush));

        const std::size_t port = GlobalQType::netw_.targetPort_[*channelPointer_];
        const bool successfulPush = globalQueue_.pushInternal(itBeginFirst, itEndFirst, targetWorker, port);
        if (successfulPush) {
            bufferTail_ += numElementsFirstPush;

            if (numElementsSecondPush > 0U) {
                const bool successfulSecondPush
                    = globalQueue_.pushInternal(outBuffer_.begin(), itEndSecond, targetWorker, port);
                if (successfulSecondPush) { bufferTail_ += numElementsSecondPush; };
            }
        }
        return successfulPush;
    }
}

/**
 * @brief Pushes all task from (including) fromPointer in the outbuffer to the local queue.
 *
 * @param numElements
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::pushOutBufferSelf(
    const std::size_t numElements) noexcept {
    constexpr bool hasBatchPush
        = requires (LocalQType &q,
                    typename decltype(outBuffer_)::iterator first,
                    typename decltype(outBuffer_)::iterator last) { q.push(first, last); };

    const std::size_t reducedTail = bufferTail_ % outBuffer_.size();
    const std::size_t numElementsFirstPush = std::min(outBuffer_.size() - reducedTail, numElements);
    const std::size_t numElementsSecondPush = numElements - numElementsFirstPush;

    const auto itBeginFirst = std::next(
        outBuffer_.begin(), static_cast<typename decltype(outBuffer_)::difference_type>(reducedTail));
    const auto itEndFirst = std::next(
        itBeginFirst, static_cast<typename decltype(outBuffer_)::difference_type>(numElementsFirstPush));
    const auto itEndSecond
        = std::next(outBuffer_.begin(),
                    static_cast<typename decltype(outBuffer_)::difference_type>(numElementsSecondPush));

    if constexpr (hasBatchPush) {
        queue_.push(itBeginFirst, itEndFirst);
        queue_.push(outBuffer_.begin(), itEndSecond);
    } else {
        for (auto it = itBeginFirst; it != itEndFirst; ++it) { queue_.push(*it); }
        for (auto it = outBuffer_.begin(); it != itEndSecond; ++it) { queue_.push(*it); }
    }

    bufferTail_ += numElements;

    // Realign outBuffer
    if constexpr (GlobalQType::netw_.gcdBatchSize() > 1U) {
        if (numElements % GlobalQType::netw_.gcdBatchSize() != 0U) [[unlikely]] {
            if (bufferTail_ == bufferHead_) [[likely]] {        // should always be the case
                const std::size_t residue = bufferTail_ % GlobalQType::netw_.gcdBatchSize();
                const std::size_t shift = (residue == 0U) ? 0U : GlobalQType::netw_.gcdBatchSize() - residue;

                bufferTail_ += shift;
                bufferHead_ += shift;
            }
        }
    }
}

/**
 * @brief Enqueues all tasks in the incomming channels into the local queue.
 *
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::enqueueInChannels() noexcept {
    for (auto &portRingBuffer : inPorts_) {
        value_type data;
        while (portRingBuffer.pop(data)) {
            queue_.push(data);
        }
    }
}

/**
 * @brief Starts running the local worker and processes the queue until the global queue is empty or stop has
 * been requested via stop token.
 *
 * @param stoken Stop token.
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
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
        pushOutBufferSelf(bufferHead_ - bufferTail_);
    }
}

/**
 * @brief Pushes a task directly into the local queue. This should never be called when the worker is
 * running/processing the global queue.
 *
 * @param val Task.
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::pushUnsafe(const value_type val) noexcept {
    queue_.push(val);
}

/**
 * @brief Returns the worker Id in the global queue.
 *
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline std::size_t WorkerResource<GlobalQType, LocalQType, numPorts>::workerId() const noexcept {
    return workerId_;
}

/**
 * @brief Increases the global count by one. Recall the global count is split between globalCount_ in the
 * global queue and localCount_ in all local queues.
 *
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::incrGlobalCount() noexcept {
    ++localCount_;
    const std::size_t qSize = queue_.size();
    if (localCount_ >= qSize) {
        const std::size_t newLocalCount = qSize / 2;
        const std::size_t diff = localCount_ - newLocalCount;

        localCount_ = newLocalCount;
        globalQueue_.globalCount_.fetch_add(diff, std::memory_order_relaxed);
        // Can be relaxed as this is only ever called during enqueueGlobal which is only ever called during
        // the processing of an element. This means we can be sure that the queue will not become empty until
        // the processing has finished by which point the increment must have occured.
    }
}

/**
 * @brief Decreases the global count by one. Recall the global count is split between globalCount_ in the
 * global queue and localCount_ in all local queues.
 *
 */
template <typename GlobalQType, BasicQueue LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::decrGlobalCount() noexcept {
    if (localCount_ == 0) {
        const std::size_t queueSize = queue_.size();
        const std::size_t newLocalCount = queueSize / 2;
        const std::size_t diff = newLocalCount + 1;

        localCount_ = newLocalCount;
        if (queueSize > 0U) [[likely]] {        // Release is only needed to communicate completed work
            globalQueue_.globalCount_.fetch_sub(diff, std::memory_order_relaxed);
        } else {
            globalQueue_.globalCount_.fetch_sub(diff, std::memory_order_release);
        }
    } else {
        --localCount_;
    }
}

}        // end namespace spapq
