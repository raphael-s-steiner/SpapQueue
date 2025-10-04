#include <benchmark/benchmark.h>

#include "RingBuffer/RingBuffer.hpp"

using namespace spapq;

constexpr std::size_t capacity = 1024U;
RingBuffer<std::size_t, capacity> channel_optional;
static void BM_RingBuffer_optional(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>( state.range(0) );
    std::srand(42U);

    const bool producer = state.thread_index() & 1;

    std::vector<std::size_t> values(N);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<std::size_t>( std::rand() );
    }

    for (auto _ : state) {
        if (producer) {
            for (std::size_t i = 0U; i < values.size(); ++i) {
                while(not channel_optional.push(values[i])) {}
            }
        } else {
            std::optional<std::size_t> popVal(std::nullopt);
            for (std::size_t i = 0U; i < values.size(); ++i) {
                while (not (popVal = channel_optional.pop())) { }
            }
            benchmark::DoNotOptimize(popVal);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.range(0) * state.iterations());
}
BENCHMARK(BM_RingBuffer_optional)->Arg(1U<<20)->Threads(2)->UseRealTime();

RingBuffer<std::size_t, capacity> channel_reference;
static void BM_RingBuffer_reference(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>( state.range(0) );
    std::srand(42U);

    const bool producer = state.thread_index() & 1;

    std::vector<std::size_t> values(N);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<std::size_t>( std::rand() );
    }

    for (auto _ : state) {
        if (producer) {
            for (std::size_t i = 0U; i < values.size(); ++i) {
                while(not channel_reference.push(values[i])) {}
            }
        } else {
            std::size_t val = 0U;
            for (std::size_t i = 0U; i < values.size(); ++i) {
                while (not channel_reference.pop(val)) { }
            }
            benchmark::DoNotOptimize(val);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.range(0) * state.iterations());
}
BENCHMARK(BM_RingBuffer_reference)->Arg(1U<<20)->Threads(2)->UseRealTime();

BENCHMARK_MAIN();