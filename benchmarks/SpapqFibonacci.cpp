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

#include <queue>
#include <vector>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"
#include "ParallelPriotityQueue/GraphExamples/LineGraph.hpp"
#include "ParallelPriotityQueue/LocalQueues/BucketQueue.hpp"
#include "ParallelPriotityQueue/LocalQueues/Fifo.hpp"
#include "ParallelPriotityQueue/SpapQueue.hpp"
#include "ParallelPriotityQueue/WorkerExamples/FibonacciWorker.hpp"

using namespace spapq;

constexpr std::size_t fibonacciTestSize = 34;

template <typename T>
using LocalQueueType = BucketQueue<T, std::greater<T>>;        // or std::priority_queue<T> or Fifo<T>

benchmark::IterationCount fibonacciProcessedElements(std::size_t N) {
    std::vector<benchmark::IterationCount> fibs(N + 1, 1);

    for (std::size_t i = fibs.size() - 3U; i < fibs.size(); --i) { fibs[i] = fibs[i + 1] + fibs[i + 2]; }

    return std::accumulate(fibs.cbegin(), fibs.cend(), 0U);
}

static void BM_Baseline_Fibonacci_1_Worker(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));

    LocalQueueType<std::size_t> queue;

    for (auto _ : state) {
        queue.push(N);
        while (queue.size() != 0U) {
            const std::size_t num = queue.top();
            queue.pop();

            if (num > 0U) { queue.push(num - 1U); }
            if (num > 1U) { queue.push(num - 2U); }
        }

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(fibonacciProcessedElements(static_cast<std::size_t>(state.range(0)))
                            * state.iterations());
}

BENCHMARK(BM_Baseline_Fibonacci_1_Worker)->Arg(fibonacciTestSize)->UseRealTime();

static void BM_SpapQueue_Fibonacci_1_Worker(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));

    constexpr std::size_t workers = 1U;
    constexpr std::size_t channels = 1U;

    constexpr std::array<std::size_t, workers + 1U> vertexPointer = {0, 1};
    constexpr std::array<std::size_t, channels> edgeTargets = {0};
    constexpr std::array<std::size_t, workers> logicalCore = {0};
    constexpr std::array<std::size_t, channels> multiplicities = {1};
    constexpr std::array<std::size_t, channels> batchSize = {8};
    constexpr std::size_t enqueueFrequency = 64;
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

    SpapQueue<std::size_t, netw, FibonacciWorker, LocalQueueType<std::size_t>> globalQ;

    for (auto _ : state) {
        state.PauseTiming();
        globalQ.initQueue();
        globalQ.pushBeforeProcessing(N, 0U);
        state.ResumeTiming();

        globalQ.processQueue();
        globalQ.waitProcessFinish();

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(fibonacciProcessedElements(static_cast<std::size_t>(state.range(0)))
                            * state.iterations());
}

BENCHMARK(BM_SpapQueue_Fibonacci_1_Worker)->Arg(fibonacciTestSize)->UseRealTime();

static void BM_SpapQueue_Fibonacci_2_Workers(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));

    constexpr std::size_t workers = 2U;
    constexpr std::size_t channels = 4U;

    constexpr std::array<std::size_t, workers + 1U> vertexPointer = {0, 2, 4};
    constexpr std::array<std::size_t, channels> edgeTargets = {0, 1, 1, 0};
    constexpr std::array<std::size_t, workers> logicalCore = {0, 1};
    constexpr std::array<std::size_t, channels> multiplicities = {2, 1, 2, 1};
    constexpr std::array<std::size_t, channels> batchSize = {8, 16, 8, 16};
    constexpr std::size_t enqueueFrequency = 64;
    constexpr std::size_t channelBufferSize = 64;
    constexpr std::size_t maxPushAttempts = 1;

    constexpr QNetwork<workers, channels> netw(vertexPointer,
                                               edgeTargets,
                                               logicalCore,
                                               multiplicities,
                                               batchSize,
                                               enqueueFrequency,
                                               channelBufferSize,
                                               maxPushAttempts);

    SpapQueue<std::size_t, netw, FibonacciWorker, LocalQueueType<std::size_t>> globalQ;

    for (auto _ : state) {
        state.PauseTiming();
        globalQ.initQueue();
        globalQ.pushBeforeProcessing(N, 0U);
        state.ResumeTiming();

        globalQ.processQueue();
        globalQ.waitProcessFinish();

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(fibonacciProcessedElements(static_cast<std::size_t>(state.range(0)))
                            * state.iterations());
}

BENCHMARK(BM_SpapQueue_Fibonacci_2_Workers)->Arg(fibonacciTestSize)->UseRealTime();

static void BM_SpapQueue_Fibonacci_4_Workers(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));

    constexpr std::size_t workers = 4U;
    constexpr std::size_t channels = 8U;

    constexpr std::size_t numaMultiplier = 16U;

    constexpr std::array<std::size_t, workers + 1U> vertexPointer = {0, 2, 4, 6, 8};
    constexpr std::array<std::size_t, channels> edgeTargets = {0, 1, 2, 3, 2, 3, 0, 1};
    constexpr std::array<std::size_t, workers> logicalCore = {0, 1, 2, 3};
    constexpr std::array<std::size_t, channels> multiplicities
        = {numaMultiplier, numaMultiplier, 1, 1, numaMultiplier, numaMultiplier, 1, 1};
    constexpr std::array<std::size_t, channels> batchSize
        = {8, 8, 8 * numaMultiplier, 8 * numaMultiplier, 8, 8, 8 * numaMultiplier, 8 * numaMultiplier};
    constexpr std::size_t enqueueFrequency = 64;
    constexpr std::size_t channelBufferSize = 8 * numaMultiplier;
    constexpr std::size_t maxPushAttempts = 1;

    constexpr QNetwork<workers, channels> netw(vertexPointer,
                                               edgeTargets,
                                               logicalCore,
                                               multiplicities,
                                               batchSize,
                                               enqueueFrequency,
                                               channelBufferSize,
                                               maxPushAttempts);

    SpapQueue<std::size_t, netw, FibonacciWorker, LocalQueueType<std::size_t>> globalQ;

    for (auto _ : state) {
        state.PauseTiming();
        globalQ.initQueue();
        globalQ.pushBeforeProcessing(N, 0U);
        state.ResumeTiming();

        globalQ.processQueue();
        globalQ.waitProcessFinish();

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(fibonacciProcessedElements(static_cast<std::size_t>(state.range(0)))
                            * state.iterations());
}

BENCHMARK(BM_SpapQueue_Fibonacci_4_Workers)->Arg(fibonacciTestSize)->UseRealTime();

static void BM_SpapQueue_Fibonacci_8_Workers(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));

    constexpr std::size_t workers = 2U;
    constexpr std::size_t channels = 4U;

    constexpr std::size_t numaMultiplier = 8U;

    constexpr std::array<std::size_t, workers + 1U> vertexPointer = {0, 2, 4};
    constexpr std::array<std::size_t, channels> edgeTargets = {0, 1, 1, 0};
    constexpr std::array<std::size_t, workers> logicalCore = {0, 1};
    constexpr std::array<std::size_t, channels> multiplicities = {1, 1, 1, 1};
    constexpr std::array<std::size_t, channels> batchSize = {8, 8 * numaMultiplier, 8, 8 * numaMultiplier};
    constexpr std::size_t enqueueFrequency = 64;
    constexpr std::size_t channelBufferSize = 8 * numaMultiplier;
    constexpr std::size_t maxPushAttempts = 1;

    constexpr QNetwork<workers, channels> netw2(vertexPointer,
                                                edgeTargets,
                                                logicalCore,
                                                multiplicities,
                                                batchSize,
                                                enqueueFrequency,
                                                channelBufferSize,
                                                maxPushAttempts);

    constexpr auto netw = LINE_GRAPH(LINE_GRAPH(netw2));

    SpapQueue<std::size_t, netw, FibonacciWorker, LocalQueueType<std::size_t>> globalQ;

    for (auto _ : state) {
        state.PauseTiming();
        globalQ.initQueue();
        globalQ.pushBeforeProcessing(N, 0U);
        state.ResumeTiming();

        globalQ.processQueue();
        globalQ.waitProcessFinish();

        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(fibonacciProcessedElements(static_cast<std::size_t>(state.range(0)))
                            * state.iterations());
}

BENCHMARK(BM_SpapQueue_Fibonacci_8_Workers)->Arg(fibonacciTestSize)->UseRealTime();

BENCHMARK_MAIN();
