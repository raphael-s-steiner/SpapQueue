/*
Copyright 2026 Raphael S. Steiner

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

#include <cassert>
#include <cmath>

#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {

template <std::size_t workers, std::size_t channels>
std::array<double, workers> workLoadDistribution(const QNetwork<workers, channels> &netw) {
    assert(netw.isValidQNetwork());
    assert(netw.isStronglyConnected());

    std::array<double, channels> weights;
    for (double &val : weights) { val = 0.0; }
    for (std::size_t worker = 0U; worker < workers; ++worker) {
        std::size_t totalWeight = 0U;

        std::size_t batchSizeGCD = 0U;
        for (std::size_t edge = netw.vertexPointer_[worker]; edge < netw.vertexPointer_[worker + 1]; ++edge) {
            batchSizeGCD = std::gcd(batchSizeGCD, netw.batchSize_[edge]);
        }

        for (std::size_t edge = netw.vertexPointer_[worker]; edge < netw.vertexPointer_[worker + 1U]; ++edge) {
            std::size_t weight = netw.multiplicities_[edge] * (netw.batchSize_[edge] / batchSizeGCD);

            totalWeight += weight;
            weights[edge] = static_cast<double>(weight);
        }

        for (std::size_t edge = netw.vertexPointer_[worker]; edge < netw.vertexPointer_[worker + 1U]; ++edge) {
            weights[edge] /= static_cast<double>(totalWeight);
        }
    }

    std::array<std::size_t, channels> targets;
    for (std::size_t edge = 0U; edge < targets.size(); ++edge) { targets[edge] = netw.target(edge); }

    std::array<double, workers> dist;
    for (double &val : dist) { val = 1.0 / static_cast<double>(workers); }

    std::array<double, workers> distAfterIteration;
    constexpr double epsilon = 1e-10;
    bool loop = true;
    while (loop) {
        loop = false;
        for (double &val : distAfterIteration) { val = 0.0; }

        for (std::size_t worker = 0U; worker < workers; ++worker) {
            for (std::size_t edge = netw.vertexPointer_[worker]; edge < netw.vertexPointer_[worker + 1U]; ++edge) {
                distAfterIteration[targets[edge]] += dist[worker] * weights[edge];
            }
        }

        for (std::size_t worker = 0U; worker < workers; ++worker) {
            if (std::abs(dist[worker] - distAfterIteration[worker]) > epsilon) {
                loop = true;
                break;
            }
        }

        std::swap(dist, distAfterIteration);
    }

    return dist;
}

}        // end namespace spapq
