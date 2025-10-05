#include <benchmark/benchmark.h>

#include "RingBuffer/RingBuffer.hpp"

using namespace spapq;

constexpr std::size_t capacity = 1024U;
RingBuffer<std::size_t, capacity> *channel_optional;
std::atomic_flag start_optional;
std::atomic_flag end_optional;

static void BM_RingBuffer_optional(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>( state.range(0) );
    std::srand(42U);

    const bool producer = state.thread_index() & 1;
    
    RingBuffer<std::size_t, capacity> chan_opt;
    if (producer) {
        start_optional.wait(false ,std::memory_order_acquire);
    } else {
        channel_optional = &chan_opt;
        start_optional.test_and_set(std::memory_order_release);
        start_optional.notify_all();
    }

    std::vector<std::size_t> values(N);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<std::size_t>( std::rand() );
    }

    for (auto _ : state) {
        if (producer) {
            for (std::size_t i = 0U; i < values.size(); ++i) {
                while(not channel_optional->push(values[i])) {}
            }
        } else {
            std::optional<std::size_t> popVal(std::nullopt);
            for (std::size_t i = 0U; i < values.size(); ++i) {
                while (not (popVal = channel_optional->pop())) { }
            }
            benchmark::DoNotOptimize(popVal);
        }
        benchmark::ClobberMemory();
    }

    if (producer) {
        end_optional.test_and_set(std::memory_order_release);
        end_optional.notify_all();
    } else {
        end_optional.wait(false, std::memory_order_acquire);
    }

    state.SetItemsProcessed(state.range(0) * state.iterations());
}
BENCHMARK(BM_RingBuffer_optional)->Arg(1U<<20)->Threads(2)->UseRealTime();

RingBuffer<std::size_t, capacity> *channel_reference;
std::atomic_flag start_reference;
std::atomic_flag end_reference;

static void BM_RingBuffer_reference(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>( state.range(0) );
    std::srand(42U);

    const bool producer = state.thread_index() & 1;

    RingBuffer<std::size_t, capacity> chan_opt;
    if (producer) {
        start_reference.wait(false ,std::memory_order_acquire);
    } else {
        channel_reference = &chan_opt;
        start_reference.test_and_set(std::memory_order_release);
        start_reference.notify_all();
    }

    std::vector<std::size_t> values(N);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<std::size_t>( std::rand() );
    }

    for (auto _ : state) {
        if (producer) {
            for (std::size_t i = 0U; i < values.size(); ++i) {
                while(not channel_reference->push(values[i])) {}
            }
        } else {
            std::size_t val = 0U;
            for (std::size_t i = 0U; i < values.size(); ++i) {
                while (not channel_reference->pop(val)) { }
            }
            benchmark::DoNotOptimize(val);
        }
        benchmark::ClobberMemory();
    }

    if (producer) {
        end_reference.test_and_set(std::memory_order_release);
        end_reference.notify_all();
    } else {
        end_reference.wait(false, std::memory_order_acquire);
    }

    state.SetItemsProcessed(state.range(0) * state.iterations());
}
BENCHMARK(BM_RingBuffer_reference)->Arg(1U<<20)->Threads(2)->UseRealTime();

BENCHMARK_MAIN();