#pragma once

#include "SpapQueueWorkerDeclaration.hpp"

namespace spapq {

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType>
void SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::waitProcessFinish() {
    for (auto &thread : workers_) { thread.join(); }
    initSync.clear();
}

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType>
template <std::size_t tupleSize, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool>>
inline bool SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::pushInternalHelper(
    const T &val, const std::size_t workerId, const std::size_t port) {
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

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType>
template <std::size_t tupleSize, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool>>
inline bool SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::pushInternalHelper(
    T &&val, const std::size_t workerId, const std::size_t port) {
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

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType>
template <std::size_t tupleSize, class InputIt, std::enable_if_t<not netw.hasHomogeneousInPorts(), bool>>
inline bool SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::pushInternalHelper(
    InputIt first, InputIt last, const std::size_t workerId, const std::size_t port) {
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

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType>
inline bool SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::pushInternal(
    const T &val, const std::size_t workerId, const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(val, port);
    } else {
        return pushInternalHelper<netw.numWorkers_>(val, workerId, port);
    }
}

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType>
inline bool SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::pushInternal(
    T &&val, const std::size_t workerId, const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(std::move(val), port);
    } else {
        return pushInternalHelper<netw.numWorkers_>(std::move(val), workerId, port);
    }
}

template <typename T,
          std::size_t workers,
          std::size_t channels,
          QNetwork<workers, channels> netw,
          template <class, class, std::size_t> class WorkerTemplate,
          typename LocalQType>
template <class InputIt>
inline bool SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::pushInternal(
    InputIt first, InputIt last, const std::size_t workerId, const std::size_t port) {
    if constexpr (netw.hasHomogeneousInPorts()) {
        return workerResources_[workerId]->push(first, last, port);
    } else {
        return pushInternalHelper<netw.numWorkers_, InputIt>(first, last, workerId, port);
    }
}

// template <typename T,
//           std::size_t workers,
//           std::size_t channels,
//           QNetwork<workers, channels> netw,
//           template <class, class, std::size_t> class WorkerTemplate,
//           typename LocalQType>
// void SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::pushUnsafe(const T &val, const
// std::size_t workerId) {
//     // TODO
// }

// template <typename T,
//           std::size_t workers,
//           std::size_t channels,
//           QNetwork<workers, channels> netw,
//           template <class, class, std::size_t> class WorkerTemplate,
//           typename LocalQType>
// void SpapQueue<T, workers, channels, netw, WorkerTemplate, LocalQType>::pushUnsafe(T &&val, const
// std::size_t workerId) {
//     // TODO
// }

}    // end namespace spapq
