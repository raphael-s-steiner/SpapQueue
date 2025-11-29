#pragma once

#include <cassert>

#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {

template <std::size_t workers, std::size_t channels>
constexpr std::size_t lineGraphNumEdges(const QNetwork<workers, channels> &qNetwork) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < workers; ++i) {
        count += (qNetwork.numPorts_[i] * (qNetwork.vertexPointer_[i + 1] - qNetwork.vertexPointer_[i]));
    }
    return count;
}

template <std::size_t workers, std::size_t channels, std::size_t outNumEdges>
consteval QNetwork<channels, outNumEdges> lineGraph(const QNetwork<workers, channels> &qNetwork) {
    assert(outNumEdges == lineGraphNumEdges(qNetwork));

    std::array<std::size_t, channels + 1U> vertPointer;
    std::array<std::size_t, outNumEdges> edgeTargets;
    std::array<std::size_t, outNumEdges> multiplicities;
    std::array<std::size_t, outNumEdges> batchSize;

    std::size_t outEdgeCount = 0U;
    vertPointer[0] = outEdgeCount;
    for (std::size_t worker = 0U; worker < qNetwork.numWorkers_; ++worker) {
        for (std::size_t edge = qNetwork.vertexPointer_[worker]; edge < qNetwork.vertexPointer_[worker + 1U];
             ++edge) {
            const std::size_t vertexJoint
                = qNetwork.edgeTargets_[edge] == qNetwork.numWorkers_ ? worker : qNetwork.edgeTargets_[edge];
            for (std::size_t tgtEdge = qNetwork.vertexPointer_[vertexJoint];
                 tgtEdge < qNetwork.vertexPointer_[vertexJoint + 1U];
                 ++tgtEdge) {
                edgeTargets[outEdgeCount] = tgtEdge;
                multiplicities[outEdgeCount]
                    = qNetwork.multiplicities_[edge] * qNetwork.multiplicities_[tgtEdge];
                batchSize[outEdgeCount] = qNetwork.batchSize_[edge];
                ++outEdgeCount;
            }
            vertPointer[edge + 1U] = outEdgeCount;
        }
    }

    auto ret = QNetwork<channels, outNumEdges>(vertPointer, edgeTargets, multiplicities, batchSize);
    return ret;
}

#define LINE_GRAPH(qNetwork)                                                                            \
    (spapq::lineGraph<qNetwork.numWorkers_, qNetwork.numChannels_, spapq::lineGraphNumEdges(qNetwork)>( \
        qNetwork))

}    // end namespace spapq
