#pragma once

#include "Discrepancy/TableGenerator.hpp"
#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {
namespace tables {

/**
 * @brief Computes the frequency of (batch-size-adjusted) pushes along outgoing channels of a worker.
 * 
 * @tparam netw QNetwork.
 * @tparam workerId Worker.
 * 
 * @see qNetworkTable
 */
template <QNetwork netw, std::size_t workerId>
constexpr std::array<std::size_t, netw.vertexPointer_[workerId + 1] - netw.vertexPointer_[workerId]>
qNetworkTableFrequencies() {
    static_assert(netw.isValidQNetwork());
    static_assert(workerId < netw.numWorkers_);

    constexpr std::size_t workerOutChannels
        = netw.vertexPointer_[workerId + 1] - netw.vertexPointer_[workerId];

    std::size_t batchSizeLCM = 1;
    for (std::size_t i = netw.vertexPointer_[workerId]; i < netw.vertexPointer_[workerId + 1]; ++i) {
        batchSizeLCM = std::lcm(batchSizeLCM, netw.batchSize_[i]);
    }

    std::array<std::size_t, workerOutChannels> frequencies;
    for (std::size_t i = netw.vertexPointer_[workerId]; i < netw.vertexPointer_[workerId + 1]; ++i) {
        const std::size_t index = i - netw.vertexPointer_[workerId];
        frequencies[index] = netw.multiplicities_[i] * (batchSizeLCM / netw.batchSize_[i]);
    }

    auto ret = reducedIntegerArray<frequencies.size()>(frequencies);
    return ret;
}

/**
 * @brief Computes the size of the channel push table of a worker in a QNetwork.
 * 
 * @tparam netw QNetwork.
 * @tparam workerId Worker.
 * 
 * @see qNetworkTable
 */
template <QNetwork netw, std::size_t workerId>
constexpr std::size_t qNetworkTableSize() {
    static_assert(workerId < netw.numWorkers_);
    std::size_t retVal = sumArray(qNetworkTableFrequencies<netw, workerId>());
    return retVal;
}

template <QNetwork netw, std::size_t workerId>
constexpr std::array<std::size_t, qNetworkTableSize<netw, workerId>()> qNetworkTable() {
    static_assert(netw.isValidQNetwork());
    static_assert(workerId < netw.numWorkers_);

    constexpr std::size_t tableLength = qNetworkTableSize<netw, workerId>();
    constexpr std::size_t workerOutChannels
        = netw.vertexPointer_[workerId + 1] - netw.vertexPointer_[workerId];

    const std::array<std::size_t, workerOutChannels> frequencies = qNetworkTableFrequencies<netw, workerId>();

    auto table = earliestDeadlineFirstTable<workerOutChannels, tableLength>(frequencies);

    for (std::size_t &val : table) { val += netw.vertexPointer_[workerId]; }

    return table;
}

/**
 * @brief Computes the maximum table size of the first N workers.
 * 
 * @tparam netw QNetwork.
 * 
 * @see qNetworkTableFrequencies
 * @see qNetworkTable
 */
template <QNetwork netw, std::size_t N>
constexpr std::size_t maxTableSizeHelper() {
    static_assert(N <= netw.numWorkers_);

    if constexpr (N == 0) {
        return 0U;
    } else {
        std::size_t retVal = std::max(qNetworkTableSize<netw, N - 1>(), maxTableSizeHelper<netw, N - 1>());
        return retVal;
    }
}

/**
 * @brief Computes the maximum table size over all workers in a QNetwork.
 * 
 * @tparam netw QNetwork.
 * 
 * @see qNetworkTableFrequencies
 * @see qNetworkTable
 */
template <QNetwork netw>
constexpr std::size_t maxTableSize() {
    return maxTableSizeHelper<netw, netw.numWorkers_>();
}

}        // end namespace tables
}        // end namespace spapq
