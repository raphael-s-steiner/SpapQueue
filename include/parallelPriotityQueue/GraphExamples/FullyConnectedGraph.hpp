#pragma once

#include "parallelPriotityQueue/QNetwork.hpp"

namespace spapq {

template<std::size_t N>
constexpr QNetwork<N, N * N> FULLY_CONNECTED_GRAPH() {
    static_assert(N > 0, "Needs to have at least one worker!");

    std::array<std::size_t, N + 1> vertexPtr;
    std::array<std::size_t, N * N> edges;

    for (std::size_t i = 0U; i < N + 1; ++i) {
        vertexPtr[i] = N * i;
    }

    for (std::size_t i = 0U; i < N * N; ++i) {
        edges[i] = i % N;
    }

    return QNetwork<N, N * N>(vertexPtr, edges);
};

} // end namespace spapq