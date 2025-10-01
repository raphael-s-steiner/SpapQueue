#pragma once

#include "Discrepancy/TableGenerator.hpp"
#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {
namespace tables {

template<std::size_t workers, std::size_t channels, QNetwork<workers, channels> netw, std::size_t worker>
constexpr std::array<std::size_t, netw.vertexPointer_[worker + 1] - netw.vertexPointer_[worker]> qNetworkTableFrequencies() {
    static_assert(netw.isValidQNetwork());
    static_assert(worker < workers);

    std::size_t batchSizeLCM = 1;
    for (std::size_t i = netw.vertexPointer_[worker]; i < netw.vertexPointer_[worker + 1]; ++i) {
        batchSizeLCM = std::lcm(batchSizeLCM, netw.batchSize_[i]);
    }

    std::array<std::size_t, netw.vertexPointer_[worker + 1] - netw.vertexPointer_[worker]> frequencies;
    for (std::size_t i = netw.vertexPointer_[worker]; i < netw.vertexPointer_[worker + 1]; ++i) {
        const std::size_t index = i - netw.vertexPointer_[worker];
        frequencies[index] = netw.multiplicities_[i] * (batchSizeLCM / netw.batchSize_[i]);
    }

    return reducedIntegerArray<frequencies.size()>(frequencies);
}

} // end namespace tables
} // end namespace spapq