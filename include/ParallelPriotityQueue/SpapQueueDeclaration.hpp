#pragma once

#include <memory>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

#include "Discrepancy/QNetworkTables.hpp"
#include "Discrepancy/TableGenerator.hpp"

namespace spapq {

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType = std::priority_queue<T>>
class SpapQueue {
    using ThisQType = SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>;

  public:
    using value_type = T;

    template <std::size_t N>
    struct WorkerCollectiveHelper;

    using WorkerCollective
        = std::conditional_t<netw.hasHomogeneousInPorts(),
                             std::array<WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[0U]> *, netw.numWorkers_>,
                             typename WorkerCollectiveHelper<netw.numWorkers_>::template partialType<>>;

    static constexpr QNetwork<workers, channels> netw_{netw};

    alignas(CACHE_LINE_SIZE)
        std::array<std::jthread, netw.numWorkers_> workers_;    // TODO does not need to be aligned
    alignas(CACHE_LINE_SIZE) WorkerCollective workerResources_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> globalCount_{0U};
    std::atomic_flag initSync;

    void initQueue();
    void pushUnsafe(const value_type &val, const std::size_t workerId = 0U);
    void pushUnsafe(value_type &&val, const std::size_t workerId = 0U);

    void processQueue() {
        initSync.test_and_set(std::memory_order_release);
        initSync.notify_all();
    };

    void waitProcessFinish();

    template <std::size_t tupleSize, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternalHelper(
        const value_type &val, const std::size_t workerId, const std::size_t port);
    template <std::size_t tupleSize, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
    [[nodiscard("Push may fail when queue is full.")]] inline bool pushInternalHelper(
        value_type &&val, const std::size_t workerId, const std::size_t port);
    template <std::size_t tupleSize, class InputIt, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
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

    static_assert(netw.isValidQNetwork(), "The QNetwork needs to be valid!");
    static_assert(std::is_same_v<value_type, typename LocalQType::value_type>,
                  "The local queue type needs to have matching value_type!");
};

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType>
template <std::size_t N>
struct SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::WorkerCollectiveHelper {
    static_assert(N <= netw.numWorkers_);
    template <typename... Args>
    using partialType = std::conditional_t<
        N == 0,
        std::tuple<Args...>,
        typename WorkerCollectiveHelper<N - 1>::
            template partialType<WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[N - 1]> *, Args...>
    >;
};

}    // end namespace spapq
