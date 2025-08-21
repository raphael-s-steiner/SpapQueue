#pragma once

#include "parallelPriotityQueue/QNetwork.hpp"

namespace spapq {

template<std::size_t workers, std::size_t channels>
constexpr std::size_t lineGraphNumEdges(const QNetwork<workers, channels> &qNetwork) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < workers; ++i) {
        count += (qNetwork.numPorts_[i] * (qNetwork.vertexPointer_[i + 1] - qNetwork.vertexPointer_[i]));
    }
    return count;
}

} // end namespace spapq