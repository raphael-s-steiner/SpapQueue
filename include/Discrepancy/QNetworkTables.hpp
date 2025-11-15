#pragma once

#include "Discrepancy/TableGenerator.hpp"
#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {
namespace tables {

template<std::size_t networkWorkers, std::size_t networkChannels, std::size_t workerOutChannels>
constexpr std::array<std::size_t, workerOutChannels> qNetworkTableFrequencies(const QNetwork<networkWorkers, networkChannels> &netw, const std::size_t worker) {
    assert(netw.isValidQNetwork());
    assert(worker < workers);
    assert(workerOutChannels == netw.vertexPointer_[worker + 1] - netw.vertexPointer_[worker]);

    std::size_t batchSizeLCM = 1;
    for (std::size_t i = netw.vertexPointer_[worker]; i < netw.vertexPointer_[worker + 1]; ++i) {
        batchSizeLCM = std::lcm(batchSizeLCM, netw.batchSize_[i]);
    }

    std::array<std::size_t, workerOutChannels> frequencies;
    for (std::size_t i = netw.vertexPointer_[worker]; i < netw.vertexPointer_[worker + 1]; ++i) {
        const std::size_t index = i - netw.vertexPointer_[worker];
        frequencies[index] = netw.multiplicities_[i] * (batchSizeLCM / netw.batchSize_[i]);
    }

    return reducedIntegerArray<frequencies.size()>(frequencies);
}

template<std::size_t networkWorkers, std::size_t networkChannels, std::size_t workerOutChannels, std::size_t tableLength>
constexpr std::array<std::size_t, tableLength> qNetworkTable(const QNetwork<networkWorkers, networkChannels> &netw, const std::size_t worker) {
    assert(netw.isValidQNetwork());
    assert(worker < workers);
    assert(workerOutChannels == netw.vertexPointer_[worker + 1] - netw.vertexPointer_[worker]);
    assert(tableLength == sumArray(qNetworkTableFrequencies(netw, worker)));

    const std::array<std::size_t, workerOutChannels> frequencies = qNetworkTableFrequencies<networkWorkers, networkChannels, workerOutChannels>(netw, worker);
    
    auto table = EarliestDeadlineFirstTable<workerOutChannels, tableLength>(frequencies);

    for (std::size_t &val : table) {
        val += netw.vertexPointer_[worker];
    }

    return table;
}

template<std::size_t networkWorkers, std::size_t networkChannels>
constexpr std::size_t maxTableSize(const QNetwork<networkWorkers, networkChannels> &netw) {
    std::size_t maxVal = 0U;

    for (std::size_t i = 0U; i < netw.numWorkers_; ++i) {
        maxVal = std::max(maxVal, sumArray(qNetworkTableFrequencies(netw, i)));
    }

    return maxVal;
}

} // end namespace tables
} // end namespace spapq