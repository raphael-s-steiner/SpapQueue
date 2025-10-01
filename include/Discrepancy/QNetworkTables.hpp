#pragma once

#include "Discrepancy/TableGenerator.hpp"
#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {
namespace tables {

template<std::size_t networkWorkers, std::size_t networkChannels, std::size_t workerOutChannels>
constexpr std::array<std::size_t, workerOutChannels> qNetworkTableFrequencies(QNetwork<networkWorkers, networkChannels> netw, std::size_t worker) {
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

} // end namespace tables
} // end namespace spapq