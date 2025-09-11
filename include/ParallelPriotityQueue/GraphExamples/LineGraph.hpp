#pragma once

#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {

template<std::size_t workers, std::size_t channels>
constexpr std::size_t lineGraphNumEdges(const QNetwork<workers, channels> &qNetwork) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < workers; ++i) {
        count += (qNetwork.numPorts_[i] * (qNetwork.vertexPointer_[i + 1] - qNetwork.vertexPointer_[i]));
    }
    return count;
}

template<std::size_t workers, std::size_t channels, std::size_t outNumEdges>
consteval QNetwork<channels, outNumEdges> lineGraph(const QNetwork<workers, channels> qNetwork) {
    std::array<std::size_t, channels + 1> vertPointer;
    std::array<std::size_t, outNumEdges> edgeTargets;
    std::array<std::size_t, outNumEdges> multiplicities;
    std::array<std::size_t, outNumEdges> batchSize;

    std::size_t outEdgeCount = 0U;
    vertPointer[0] = outEdgeCount;
    for (std::size_t edge = 0U; edge < qNetwork.numChannels_; ++edge) {
        const std::size_t vertexJoint = qNetwork.edgeTargets_[edge];
        for (std::size_t tgtEdge = qNetwork.vertexPointer_[vertexJoint]; tgtEdge < qNetwork.vertexPointer_[vertexJoint + 1]; ++tgtEdge) {
            edgeTargets[outEdgeCount] = tgtEdge;
            multiplicities[outEdgeCount] = qNetwork.multiplicities_[edge] * qNetwork.multiplicities_[tgtEdge];
            batchSize[outEdgeCount] = qNetwork.batchSize_[edge];
            ++outEdgeCount;
        }
        vertPointer[edge + 1] = outEdgeCount;
    }

    return QNetwork<channels, outNumEdges>(vertPointer, edgeTargets, multiplicities, batchSize);
}

#define LINE_GRAPH(qNetwork) (lineGraph<qNetwork.numWorkers_, qNetwork.numChannels_, lineGraphNumEdges(qNetwork)>(qNetwork))

} // end namespace spapq