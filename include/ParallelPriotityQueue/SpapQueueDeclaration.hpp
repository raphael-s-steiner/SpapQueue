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

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
class WorkerResource;

template <template <class, class, std::size_t> class WorkerTemplate,
          class GlobalQType,
          class LocalQType,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          std::size_t N>
struct WorkerCollectiveHelper {
    static_assert(N <= netw.numWorkers_);
    template <typename... Args>
    using type =
        typename WorkerCollectiveHelper<WorkerTemplate, GlobalQType, LocalQType, workers, channels, netw, N - 1>::
            template type<WorkerTemplate<GlobalQType, LocalQType, netw.numPorts_[N - 1]> *, Args...>;
};

template <template <class, class, std::size_t> class WorkerTemplate,
          class GlobalQType,
          class LocalQType,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw>
struct WorkerCollectiveHelper<WorkerTemplate, GlobalQType, LocalQType, workers, channels, netw, 0> {
    template <typename... Args>
    using type = std::tuple<Args...>;
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

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType = std::priority_queue<T>>
class SpapQueue {
    using ThisQType = SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>;

    template <typename, typename, std::size_t>
    friend class WorkerTemplate;

  public:
    using value_type = T;

    using WorkerCollective = std::conditional_t<
        netw.hasHomogeneousInPorts(),
        std::array<WorkerTemplate<ThisQType, LocalQType, netw.numPorts_[0U]> *, netw.numWorkers_>,
        typename WorkerCollectiveHelper<WorkerTemplate, ThisQType, LocalQType, workers, channels, netw, netw.numWorkers_>::
            template type<>
    >;

    static constexpr QNetwork<workers, channels> netw_{netw};

    alignas(CACHE_LINE_SIZE)
        std::array<std::jthread, netw.numWorkers_> workers_;    // TODO does not need to be aligned
    alignas(CACHE_LINE_SIZE) WorkerCollective workerResources_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> globalCount_{0U};
    std::atomic_flag initSync;

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

    static_assert(netw.isValidQNetwork(), "The QNetwork needs to be valid!");
    static_assert(std::is_same_v<value_type, typename LocalQType::value_type>,
                  "The local queue type needs to have matching value_type!");
    static_assert(isDerivedWorkerResource<WorkerTemplate, ThisQType, LocalQType, netw.numWorkers_>(),
                  "WorkerTemplate must be derived from WorkerResource.");
};

}    // end namespace spapq
