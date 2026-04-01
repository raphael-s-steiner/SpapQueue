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

#include "ParallelPriorityQueue/QNetwork/QNetwork.hpp"

namespace spapq {

/**
 * @brief Combines two QNetworks with the same worker count by taking the union of the two underlying graphs.
 * The enqueue frequency, channel buffer size, max push attempts, and the logical core association is taken
 * from the first QNetwork.
 *
 * This implementation does not merge two channels with the same target.
 *
 * @tparam workers Number of workers
 * @tparam channels1 Number of channels of the first QNetwork
 * @tparam channels2 Number of channels of the second QNetwork
 * @param netw1 First QNetwork
 * @param netw2 Second QNetwork
 * @param weight1 multiplier of the multiplicities of the first QNetwork
 * @param weight2 multiplier of the multiplicities of the second QNetwork
 * @return constexpr QNetwork<workers, channels1 + channels2> Combined QNetwork
 *
 * @see QNetwork
 */
template <std::size_t workers, std::size_t channels1, std::size_t channels2>
constexpr QNetwork<workers, channels1 + channels2> combine(
    const QNetwork<workers, channels1> &netw1,
    const QNetwork<workers, channels2> &netw2,
    const std::size_t weight1 = 1U,
    const std::size_t weight2 = 1U
) {
    // does not merge channels with the same target
    constexpr std::size_t channels = channels1 + channels2;

    // copies information from the first network
    const std::size_t enqueueFrequency = netw1.enqueueFrequency_;
    const std::size_t channelBufferSize = netw1.channelBufferSize_;
    const std::size_t maxPushAttempts = netw1.maxPushAttempts_;
    const std::array<std::size_t, workers> logicalCore = netw1.logicalCore_;

    // combines the edges
    std::array<std::size_t, workers + 1U> vertexPointers = {};
    std::array<std::size_t, channels> edgeTargets = {};
    std::array<std::size_t, channels> multiplicities = {};
    std::array<std::size_t, channels> batchSize = {};

    vertexPointers[0U] = 0U;
    for (std::size_t worker = 0U, worker < workers; ++worker) {
        const std::size_t outDegree = netw1.outDegree(worker) + netw2.outDegree(worker);
        vertexPointers[worker + 1U] = vertexPointers[worker] + outDegree;

        std::size_t gcd = 0U;
        std::size_t edgeIdx = vertexPointers[worker];
        for (std::size_t e1 = netw1.vertexPointer_[worker]; e1 < netw1.vertexPointer_[worker + 1U]; ++e1) {
            edgeTargets[edgeIdx] = netw1.edgeTargets_[e1];
            multiplicities[edgeIdx] = weight1 * netw1.multiplicities_[e1];
            gcd = std::gcd(gcd, multiplicities[edgeIdx]);
            batchSize[edgeIdx] = netw1.batchSize[e1];
            ++edgeIdx;
        }
        for (std::size_t e2 = netw2.vertexPointer_[worker]; e2 < netw2.vertexPointer_[worker + 1U]; ++e2) {
            edgeTargets[edgeIdx] = netw2.edgeTargets_[e2];
            multiplicities[edgeIdx] = weight2 * netw2.multiplicities_[e2];
            gcd = std::gcd(gcd, multiplicities[edgeIdx]);
            batchSize[edgeIdx] = netw2.batchSize[e2];
            ++edgeIdx;
        }

        for (std::size_t e = vertexPointers[worker]; e < vertexPointers[worker + 1U]; ++e) {
            multiplicities[e] /= gcd;
        }
    }

    return QNetwork<workers, channels>(
        vertexPointers,
        edgeTargets,
        logicalCore,
        multiplicities,
        batchSize,
        enqueueFrequency,
        channelBufferSize,
        maxPushAttempts
    );
}

}        // end namespace spapq
