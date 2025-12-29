/*
Copyright 2025 Raphael S. Steiner

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

@author Raphael S. Steiner
*/

#pragma once

#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {

/**
 * @brief A fully connected QNetwork including all self-loops.
 *
 * @tparam N Number of workers.
 *
 * @see QNetwork
 */
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
