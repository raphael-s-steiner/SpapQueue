#pragma once

#include <iterator>
#include <stop_token>

#include "Discrepancy/QNetworkTables.hpp"
#include "Discrepancy/TableGenerator.hpp"
#include "RingBuffer/RingBuffer.hpp"

namespace spapq {

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
class WorkerResource {
    template <typename, typename, std::size_t>
    friend class WorkerResource;
    template <typename, QNetwork, template <class, class, std::size_t> class, typename>
    friend class SpapQueue;

  public:
    using value_type = GlobalQType::value_type;

  private:
    const std::array<std::size_t, tables::maxTableSize<GlobalQType::netw_>()> channelIndices_;
    std::array<value_type, GlobalQType::netw_.maxBatchSize()> outBuffer_;

    alignas(CACHE_LINE_SIZE) GlobalQType &globalQueue_;
    typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator bufferPointer_;
    typename std::array<std::size_t, tables::maxTableSize<GlobalQType::netw_>()>::const_iterator channelPointer_;
    const typename std::array<std::size_t, tables::maxTableSize<GlobalQType::netw_>()>::const_iterator
        channelTableEndPointer_;

    std::array<RingBuffer<value_type, GlobalQType::netw_.bufferSize_>, numPorts> inPorts_;
    LocalQType queue_;

    [[nodiscard("Push may fail when queue is full.\n")]] inline bool pushOutBuffer();
    inline void pushOutBufferSelf(
        const typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator fromPointer);

    inline void enqueueInChannels();
    virtual void processElement(const value_type &val) = 0;

    [[nodiscard("Push may fail when queue is full.\n")]] inline bool push(const value_type &val,
                                                                          std::size_t port);
    [[nodiscard("Push may fail when queue is full.\n")]] inline bool push(value_type &&val, std::size_t port);
    template <class InputIt>
    [[nodiscard("Push may fail when queue is full.\n")]] inline bool push(InputIt first,
                                                                          InputIt last,
                                                                          std::size_t port);

    inline void pushUnsafe(const value_type &val);
    inline void pushUnsafe(value_type &&val);

    inline void run(std::stop_token stoken);

  protected:
    inline void enqueueGlobal(const value_type &val);

    template <std::size_t channelIndicesLength>
    constexpr WorkerResource(GlobalQType &globalQueue,
                             const std::array<std::size_t, channelIndicesLength> &channelIndices);

  public:
    WorkerResource(const WorkerResource &other) = delete;
    WorkerResource(WorkerResource &&other) = delete;
    WorkerResource &operator=(const WorkerResource &other) = delete;
    WorkerResource &operator=(WorkerResource &&other) = delete;
    virtual ~WorkerResource() = default;
};

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

// Implementation details

template <template <class, class, std::size_t> class WorkerTemplate,
          class GlobalQType,
          class LocalQType,
          QNetwork netw,
          std::size_t N>
struct WorkerCollectiveHelper {
    static_assert(N <= netw.numWorkers_);
    template <typename... Args>
    using type = typename WorkerCollectiveHelper<WorkerTemplate, GlobalQType, LocalQType, netw, N - 1>::
        template type<WorkerTemplate<GlobalQType, LocalQType, netw.numPorts_[N - 1]> *, Args...>;
};

template <template <class, class, std::size_t> class WorkerTemplate, class GlobalQType, class LocalQType, QNetwork netw>
struct WorkerCollectiveHelper<WorkerTemplate, GlobalQType, LocalQType, netw, 0> {
    template <typename... Args>
    using type = std::tuple<Args...>;
};

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
template <std::size_t channelIndicesLength>
constexpr WorkerResource<GlobalQType, LocalQType, numPorts>::WorkerResource(
    GlobalQType &globalQueue, const std::array<std::size_t, channelIndicesLength> &channelIndices) :
    channelIndices_(
        tables::extendTable<tables::maxTableSize<GlobalQType::netw_>(), channelIndicesLength>(channelIndices)),
    globalQueue_(globalQueue),
    bufferPointer_(outBuffer_.begin()),
    channelPointer_(channelIndices_.cbegin()),
    channelTableEndPointer_(std::next(channelIndices_.cbegin(), channelIndicesLength)) { }

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
    while (static_cast<std::size_t>(std::distance(outBuffer_.begin(), bufferPointer_))
               >= GlobalQType::netw_.batchSize_[*channelPointer_]
           && maxAttempts > 0U) {
        if (not pushOutBuffer()) { --maxAttempts; }

        ++channelPointer_;
        if (channelPointer_ == channelTableEndPointer_) { channelPointer_ = channelIndices_.cbegin(); }
    }
    if (maxAttempts == 0U) [[unlikely]] { pushOutBufferSelf(outBuffer_.begin()); }
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline bool WorkerResource<GlobalQType, LocalQType, numPorts>::pushOutBuffer() {
    bool successfulPush = false;

    const std::size_t batch = GlobalQType::netw_.batchSize_[*channelPointer_];
    assert(batch <= static_cast<std::size_t>(std::distance(outBuffer_.begin(), bufferPointer_)));
    const typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator itBegin = std::prev(
        bufferPointer_,
        static_cast<typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::difference_type>(
            batch));

    const std::size_t targetWorker = GlobalQType::netw_.edgeTargets_[*channelPointer_];
    if (targetWorker == GlobalQType::netw_.numWorkers_) {    // netw.numWorkers_ is reserved for self-push
        pushOutBufferSelf(itBegin);
        successfulPush = true;
    } else {
        const std::size_t port = GlobalQType::netw_.targetPort_[*channelPointer_];
        successfulPush = globalQueue_.pushInternal(itBegin, bufferPointer_, targetWorker, port);
        if (successfulPush) { bufferPointer_ = itBegin; }
    }

    return successfulPush;
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::pushOutBufferSelf(
    const typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator fromPointer) {
    constexpr bool hasBatchPush
        = requires (LocalQType &q,
                    typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator first,
                    typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator last) {
              q.push(first, last);
          };

    if constexpr (hasBatchPush) {
        queue_.push(fromPointer, bufferPointer_);
    } else {
        for (auto it = fromPointer; it != bufferPointer_; ++it) { queue_.push(*it); }
    }
    bufferPointer_ = fromPointer;
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::enqueueInChannels() {
    for (auto &portRingBuffer : inPorts_) {
        std::optional<value_type> data = portRingBuffer.pop();
        while (data.has_value()) {
            queue_.push(data.value());
            data = portRingBuffer.pop();
        }
    }
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::run(std::stop_token stoken) {
    std::size_t cntr = 0;
    while (globalQueue_.globalCount_.load(std::memory_order_acquire) > 0 && (not stoken.stop_requested())) {
        while ((not queue_.empty())) [[likely]] {
            if (cntr % 128U == 0U) {
                if (stoken.stop_requested()) [[unlikely]] { break; }
            }

            if (cntr % GlobalQType::netw_.enqueueFrequency_ == 0U) { enqueueInChannels(); }

            const value_type &val = queue_.top();
            processElement(val);
            queue_.pop();
            globalQueue_.globalCount_.fetch_sub(1U, std::memory_order_release);

            ++cntr;
        }
        enqueueInChannels();
        pushOutBufferSelf(outBuffer_.begin());
    }
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::pushUnsafe(const value_type &val) {
    queue_.push(val);
}

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
inline void WorkerResource<GlobalQType, LocalQType, numPorts>::pushUnsafe(value_type &&val) {
    queue_.push(std::move(val));
}

}    // end namespace spapq
