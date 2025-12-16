#include <benchmark/benchmark.h>

#include <vector>

#include "ParallelPriotityQueue/SpapQueue.hpp"
#include "ParallelPriotityQueue/WorkerExamples/FibonacciWorker.hpp"

using namespace spapq;

constexpr std::size_t fibonacciTestSize = 30;

std::size_t fibonacciProcessedElements(std::size_t N) { 
    std::vector<std::size_t> fibs(N+1, 1U);

    for (std::size_t i = fibs.size() - 3U; i < fibs.size(); --i) {
        fibs[i] = fibs[i + 1] + fibs[i + 2];
    }

    return std::accumulate(fibs.cbegin(), fibs.cend(), 0U);
}

static void BM_SpapQueue_Fibonacci_1Worker(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));

    constexpr QNetwork<1, 1> netw = FULLY_CONNECTED_GRAPH<1U>();

    SpapQueue<std::size_t, netw, FibonacciWorker, std::priority_queue<std::size_t>> globalQ;
    globalQ.initQueue();
    globalQ.pushUnsafe(fibonacciTestSize, 0U);

    for (auto _ : state) {
        // TODO
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(fibonacciProcessedElements(state.range(0)) * state.iterations());
}

BENCHMARK(BM_SpapQueue_Fibonacci_1Worker)->Arg(fibonacciTestSize)->UseRealTime();

BENCHMARK_MAIN();
