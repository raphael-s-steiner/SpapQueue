#pragma once

#include <memory>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "Discrepancy/QNetworkTables.hpp"
#include "Discrepancy/TableGenerator.hpp"

namespace spapq {

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          typename LocalQType = std::priority_queue<T>>
class SpapQueue {
  public:
    template <std::size_t numPorts, std::size_t tableLength>
    class WorkerResource;

    template <std::size_t N>
    struct WorkerCollectiveHelper;

    using WorkerCollective
        = std::conditional_t<netw.hasHomogeneousInPorts(),
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
        initSync.test_and_set(std::memory_order_release);
        initSync.notify_all();
    };

    void waitProcessFinish();

    template <std::size_t tupleSize, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternalHelper(
        const T &val, const std::size_t workerId, const std::size_t port);
    template <std::size_t tupleSize, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternalHelper(
        T &&val, const std::size_t workerId, const std::size_t port);
    template <std::size_t tupleSize, class InputIt, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool> = true>
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternalHelper(
        InputIt first, InputIt last, const std::size_t workerId, const std::size_t port);

    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternal(const T &val,
                                                                               const std::size_t workerId,
                                                                               const std::size_t port);
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternal(T &&val,
                                                                               const std::size_t workerId,
                                                                               const std::size_t port);
    template <class InputIt>
    [[nodiscard("Push may fail when queue is full")]] inline bool pushInternal(InputIt first,
                                                                               InputIt last,
                                                                               const std::size_t workerId,
                                                                               const std::size_t port);

    static_assert(netw.isValidQNetwork(), "The QNetwork needs to be valid!");
    static_assert(std::is_same_v<T, typename LocalQType::value_type>,
                  "The Local Queue Type needs to have matching value_type!");
};

template <typename T, std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, typename LocalQType>
template <std::size_t N>
struct SpapQueue<T, workers, channels, netw, LocalQType>::WorkerCollectiveHelper {
    static_assert(N <= netw.numWorkers_);
    template <typename... Args>
    using partialType = std::conditional_t<
        N == 0,
        std::tuple<Args...>,
        typename WorkerCollectiveHelper<N - 1>::template partialType<
            std::unique_ptr<WorkerResource<netw.numPorts_[N - 1], sumArray(qNetworkTableFrequencies(netw, N - 1))>>,
            Args...
        >
    >;
};

}    // end namespace spapq
