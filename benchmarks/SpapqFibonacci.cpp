#include <benchmark/benchmark.h>

#include <queue>
#include <vector>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"
#include "ParallelPriotityQueue/GraphExamples/LineGraph.hpp"
#include "ParallelPriotityQueue/SpapQueue.hpp"
#include "ParallelPriotityQueue/WorkerExamples/FibonacciWorker.hpp"

using namespace spapq;

constexpr std::size_t fibonacciTestSize = 34;

benchmark::IterationCount fibonacciProcessedElements(std::size_t N) {
    std::vector<benchmark::IterationCount> fibs(N + 1, 1);

    for (std::size_t i = fibs.size() - 3U; i < fibs.size(); --i) { fibs[i] = fibs[i + 1] + fibs[i + 2]; }

    return std::accumulate(fibs.cbegin(), fibs.cend(), 0U);
}

static void BM_SpapQueue_Fibonacci_1_Worker(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));

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

    SpapQueue<std::size_t, netw, FibonacciWorker, std::priority_queue<std::size_t>> globalQ;

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

    SpapQueue<std::size_t, netw, FibonacciWorker, std::priority_queue<std::size_t>> globalQ;

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

    SpapQueue<std::size_t, netw, FibonacciWorker, std::priority_queue<std::size_t>> globalQ;

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

    SpapQueue<std::size_t, netw, FibonacciWorker, std::priority_queue<std::size_t>> globalQ;

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
