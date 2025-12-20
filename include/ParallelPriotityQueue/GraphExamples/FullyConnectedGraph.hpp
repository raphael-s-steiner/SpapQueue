#pragma once

#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {

template <std::size_t N>
consteval QNetwork<N, N * N> FULLY_CONNECTED_GRAPH() {
    static_assert(N > 0U, "Needs to have at least one worker!");

    std::array<std::size_t, N + 1U> vertexPtr;
    std::array<std::size_t, N * N> edges;

    for (std::size_t i = 0U; i < N + 1U; ++i) { vertexPtr[i] = N * i; }

    for (std::size_t i = 0U; i < N * N; ++i) { edges[i] = (i + (i / N)) % N; }

    return QNetwork<N, N * N>(vertexPtr, edges);
};

}        // end namespace spapq
