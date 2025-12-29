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

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <random>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"
#include "ParallelPriotityQueue/GraphExamples/LineGraph.hpp"
#include "ParallelPriotityQueue/SpapQueue.hpp"
#include "ParallelPriotityQueue/WorkerExamples/SSSPWorker.hpp"

using namespace spapq;

constexpr unsigned numVertices_ = 20000U;
constexpr unsigned edgesPerVertex_ = 7U;
constexpr std::size_t seedNumber_ = 1729U;

CSRGraph makeGraph(const unsigned numVertices, const unsigned edgesPerVertex, const std::size_t seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    std::vector<double> xAxis(numVertices);
    std::vector<double> yAxis(numVertices);

    for (unsigned v = 0U; v < numVertices; ++v) {
        xAxis[v] = dis(gen);
        yAxis[v] = dis(gen);
    }

    std::vector<std::vector<unsigned>> tempGraph(numVertices);

    for (unsigned v = 0U; v < numVertices; ++v) {
        std::priority_queue<std::pair<double, unsigned>> closestPts;
        for (unsigned w = 0U; w < v; ++w) {
            const double dist = std::hypot(xAxis[w] - xAxis[v], yAxis[w] - yAxis[v]);

            if (closestPts.size() < edgesPerVertex) {
                closestPts.emplace(dist, w);
            } else if (closestPts.top().first > dist) {
                closestPts.pop();
                closestPts.emplace(dist, w);
            }
        }

        while (not closestPts.empty()) {
            const unsigned w = closestPts.top().second;

            tempGraph[w].emplace_back(v);
            tempGraph[v].emplace_back(w);

            closestPts.pop();
        }
    }

    for (unsigned v = 0U; v < numVertices; ++v) { std::sort(tempGraph[v].begin(), tempGraph[v].end()); }

    CSRGraph graph;
    graph.sourcePointers_.reserve(numVertices + 1U);
    graph.edgeTargets_.reserve(numVertices * edgesPerVertex * 2U);

    for (unsigned v = 0U; v < numVertices; ++v) {
        graph.sourcePointers_.emplace_back(graph.edgeTargets_.size());
        for (unsigned w : tempGraph[v]) { graph.edgeTargets_.emplace_back(w); }
    }
    graph.sourcePointers_.emplace_back(graph.edgeTargets_.size());

    return graph;
};

static void BM_SpapQueue_SSSP_1_Worker(benchmark::State &state) {
    constexpr std::size_t workers = 1U;
    constexpr std::size_t channels = 1U;

    constexpr std::array<std::size_t, workers + 1U> vertexPointer = {0, 1};
    constexpr std::array<std::size_t, channels> edgeTargets = {0};
    constexpr std::array<std::size_t, workers> logicalCore = {0};
    constexpr std::array<std::size_t, channels> multiplicities = {1};
    constexpr std::array<std::size_t, channels> batchSize = {8};
    constexpr std::size_t enqueueFrequency = 24;
    constexpr std::size_t channelBufferSize = 8;
    constexpr std::size_t maxPushAttempts = 1;

    constexpr QNetwork<workers, channels> netw(vertexPointer,
                                               edgeTargets,
                                               logicalCore,
                                               multiplicities,
                                               batchSize,
                                               enqueueFrequency,
                                               channelBufferSize,
                                               maxPushAttempts);

    SpapQueue<std::pair<unsigned, unsigned>,
              netw,
              SSSPWorker,
              std::priority_queue<std::pair<unsigned, unsigned>,
                                  std::vector<std::pair<unsigned, unsigned>>,
                                  std::greater<std::pair<unsigned, unsigned>>>>
        globalQ;

    const unsigned nVerts = static_cast<unsigned>(state.range(0));
    const unsigned ePerVerts = static_cast<unsigned>(state.range(1));
    const std::size_t seed = static_cast<std::size_t>(state.range(2));

    const CSRGraph graph = makeGraph(nVerts, ePerVerts, seed);
    std::vector<std::atomic<unsigned>> distances(nVerts);

    for (auto _ : state) {
        state.PauseTiming();

        for (auto &dist : distances) {
            dist.store(std::numeric_limits<unsigned>::max(), std::memory_order_relaxed);
        }

        globalQ.initQueue(std::cref(graph), std::ref(distances));
        globalQ.pushBeforeProcessing({0, 0}, 0U);

        state.ResumeTiming();

        globalQ.processQueue();
        globalQ.waitProcessFinish();

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.range(0) * state.iterations());
}

BENCHMARK(BM_SpapQueue_SSSP_1_Worker)->Args({numVertices_, edgesPerVertex_, seedNumber_})->UseRealTime();

static void BM_SpapQueue_SSSP_2_Workers(benchmark::State &state) {
    constexpr std::size_t workers = 2U;
    constexpr std::size_t channels = 4U;

    constexpr std::array<std::size_t, workers + 1U> vertexPointer = {0, 2, 4};
    constexpr std::array<std::size_t, channels> edgeTargets = {0, 1, 1, 0};
    constexpr std::array<std::size_t, workers> logicalCore = {0, 1};
    constexpr std::array<std::size_t, channels> multiplicities = {2, 1, 2, 1};
    constexpr std::array<std::size_t, channels> batchSize = {8, 16, 8, 16};
    constexpr std::size_t enqueueFrequency = 24;
    constexpr std::size_t channelBufferSize = 64;
    constexpr std::size_t maxPushAttempts = 2;

    constexpr QNetwork<workers, channels> netw(vertexPointer,
                                               edgeTargets,
                                               logicalCore,
                                               multiplicities,
                                               batchSize,
                                               enqueueFrequency,
                                               channelBufferSize,
                                               maxPushAttempts);

    SpapQueue<std::pair<unsigned, unsigned>,
              netw,
              SSSPWorker,
              std::priority_queue<std::pair<unsigned, unsigned>,
                                  std::vector<std::pair<unsigned, unsigned>>,
                                  std::greater<std::pair<unsigned, unsigned>>>>
        globalQ;

    const unsigned nVerts = static_cast<unsigned>(state.range(0));
    const unsigned ePerVerts = static_cast<unsigned>(state.range(1));
    const std::size_t seed = static_cast<std::size_t>(state.range(2));

    const CSRGraph graph = makeGraph(nVerts, ePerVerts, seed);
    std::vector<std::atomic<unsigned>> distances(nVerts);

    for (auto _ : state) {
        state.PauseTiming();

        for (auto &dist : distances) {
            dist.store(std::numeric_limits<unsigned>::max(), std::memory_order_relaxed);
        }

        globalQ.initQueue(std::cref(graph), std::ref(distances));
        globalQ.pushBeforeProcessing({0, 0}, 0U);

        state.ResumeTiming();

        globalQ.processQueue();
        globalQ.waitProcessFinish();

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.range(0) * state.iterations());
}

BENCHMARK(BM_SpapQueue_SSSP_2_Workers)->Args({numVertices_, edgesPerVertex_, seedNumber_})->UseRealTime();

static void BM_SpapQueue_SSSP_4_Workers(benchmark::State &state) {
    constexpr std::size_t workers = 4U;
    constexpr std::size_t channels = 8U;

    constexpr std::array<std::size_t, workers + 1U> vertexPointer = {0, 2, 4, 6, 8};
    constexpr std::array<std::size_t, channels> edgeTargets = {0, 1, 2, 3, 2, 3, 0, 1};
    constexpr std::array<std::size_t, workers> logicalCore = {0, 1, 2, 3};
    constexpr std::array<std::size_t, channels> multiplicities = {2, 2, 1, 1, 2, 2, 1, 1};
    constexpr std::array<std::size_t, channels> batchSize = {8, 8, 16, 16, 8, 8, 16, 16};
    constexpr std::size_t enqueueFrequency = 24;
    constexpr std::size_t channelBufferSize = 64;
    constexpr std::size_t maxPushAttempts = 2;

    constexpr QNetwork<workers, channels> netw(vertexPointer,
                                               edgeTargets,
                                               logicalCore,
                                               multiplicities,
                                               batchSize,
                                               enqueueFrequency,
                                               channelBufferSize,
                                               maxPushAttempts);

    SpapQueue<std::pair<unsigned, unsigned>,
              netw,
              SSSPWorker,
              std::priority_queue<std::pair<unsigned, unsigned>,
                                  std::vector<std::pair<unsigned, unsigned>>,
                                  std::greater<std::pair<unsigned, unsigned>>>>
        globalQ;

    const unsigned nVerts = static_cast<unsigned>(state.range(0));
    const unsigned ePerVerts = static_cast<unsigned>(state.range(1));
    const std::size_t seed = static_cast<std::size_t>(state.range(2));

    const CSRGraph graph = makeGraph(nVerts, ePerVerts, seed);
    std::vector<std::atomic<unsigned>> distances(nVerts);

    for (auto _ : state) {
        state.PauseTiming();

        for (auto &dist : distances) {
            dist.store(std::numeric_limits<unsigned>::max(), std::memory_order_relaxed);
        }

        globalQ.initQueue(std::cref(graph), std::ref(distances));
        globalQ.pushBeforeProcessing({0, 0}, 0U);

        state.ResumeTiming();

        globalQ.processQueue();
        globalQ.waitProcessFinish();

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.range(0) * state.iterations());
}

BENCHMARK(BM_SpapQueue_SSSP_4_Workers)->Args({numVertices_, edgesPerVertex_, seedNumber_})->UseRealTime();

static void BM_SpapQueue_SSSP_8_Workers(benchmark::State &state) {
    constexpr std::size_t workers = 2U;
    constexpr std::size_t channels = 4U;

    constexpr std::array<std::size_t, workers + 1U> vertexPointer = {0, 2, 4};
    constexpr std::array<std::size_t, channels> edgeTargets = {0, 1, 1, 0};
    constexpr std::array<std::size_t, workers> logicalCore = {0, 1};
    constexpr std::array<std::size_t, channels> multiplicities = {1, 1, 1, 1};
    constexpr std::array<std::size_t, channels> batchSize = {8, 16, 8, 16};
    constexpr std::size_t enqueueFrequency = 24;
    constexpr std::size_t channelBufferSize = 64;
    constexpr std::size_t maxPushAttempts = 4;

    constexpr QNetwork<workers, channels> netw2(vertexPointer,
                                                edgeTargets,
                                                logicalCore,
                                                multiplicities,
                                                batchSize,
                                                enqueueFrequency,
                                                channelBufferSize,
                                                maxPushAttempts);

    constexpr auto netw = LINE_GRAPH(LINE_GRAPH(netw2));

    SpapQueue<std::pair<unsigned, unsigned>,
              netw,
              SSSPWorker,
              std::priority_queue<std::pair<unsigned, unsigned>,
                                  std::vector<std::pair<unsigned, unsigned>>,
                                  std::greater<std::pair<unsigned, unsigned>>>>
        globalQ;

    const unsigned nVerts = static_cast<unsigned>(state.range(0));
    const unsigned ePerVerts = static_cast<unsigned>(state.range(1));
    const std::size_t seed = static_cast<std::size_t>(state.range(2));

    const CSRGraph graph = makeGraph(nVerts, ePerVerts, seed);
    std::vector<std::atomic<unsigned>> distances(nVerts);

    for (auto _ : state) {
        state.PauseTiming();

        for (auto &dist : distances) {
            dist.store(std::numeric_limits<unsigned>::max(), std::memory_order_relaxed);
        }

        globalQ.initQueue(std::cref(graph), std::ref(distances));
        globalQ.pushBeforeProcessing({0, 0}, 0U);

        state.ResumeTiming();

        globalQ.processQueue();
        globalQ.waitProcessFinish();

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.range(0) * state.iterations());
}

BENCHMARK(BM_SpapQueue_SSSP_8_Workers)->Args({numVertices_, edgesPerVertex_, seedNumber_})->UseRealTime();

BENCHMARK_MAIN();
