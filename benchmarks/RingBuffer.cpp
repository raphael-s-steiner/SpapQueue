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

#include "RingBuffer/RingBuffer.hpp"

#include <benchmark/benchmark.h>
#include <pthread.h>

#include <iostream>

using namespace spapq;

constexpr std::size_t capacity = 1U << 10;
constexpr int64_t numItems = 1 << 20;
constexpr unsigned seed = 42U;
constexpr bool randomValues = false;

constexpr std::size_t producerCpu = 0;
constexpr std::size_t consumerCpu = 1;

void pin_thread(std::size_t cpu) {
    pthread_t self = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    const int rc = pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        const std::string errorMessage = "Call to pthread_setaffinity_np returned error "
                                            + std::to_string(rc)
                                            + ".\nFailed to pin thread to core "
                                            + std::to_string(cpu)
                                            + ".\n";
        std::cerr << errorMessage;
        std::exit(EXIT_FAILURE);
    }
}

static void BM_RingBuffer_1Threads_alternating(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::srand(seed);

    RingBuffer<std::size_t, capacity> channel;

    std::vector<std::size_t> values(N);
    for (std::size_t i = 0; i < values.size(); ++i) { values[i] = static_cast<std::size_t>(std::rand()); }

    for (auto _ : state) {
        for (std::size_t i = 0U; i < values.size(); ++i) {
            if constexpr (randomValues) {
                while (not channel.push(values[i])) { }
            } else {
                while (not channel.push(i)) { }
            }
            std::size_t val = 0U;
            while (not channel.pop(val)) { }
            benchmark::DoNotOptimize(val);
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.range(0) * state.iterations());
}

BENCHMARK(BM_RingBuffer_1Threads_alternating)->Arg(numItems)->UseRealTime();

static void BM_RingBuffer_1Threads_random(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::srand(seed);

    RingBuffer<std::size_t, capacity> channel;

    std::vector<std::size_t> values(N);
    for (std::size_t i = 0; i < values.size(); ++i) { values[i] = static_cast<std::size_t>(std::rand()); }

    for (auto _ : state) {
        std::size_t pushCntr = 0U;
        std::size_t popCntr = 0U;
        while (pushCntr < values.size() || popCntr < values.size()) {
            for (std::size_t j = pushCntr;; ++j) {
                bool producer;
                if constexpr (randomValues) {
                    if (j == values.size()) { j = 0U; }
                    producer = values[j] % 2U == 0;
                } else {
                    producer = ((j * static_cast<std::size_t>(691U)) & static_cast<std::size_t>(8U)) == static_cast<std::size_t>(0U);
                }

                std::size_t val = 0U;
                if (producer) {
                    if constexpr (randomValues) {
                        if (pushCntr < values.size() && channel.push((values[pushCntr]))) {
                            ++pushCntr;
                            break;
                        }
                    } else {
                        if (pushCntr < values.size() && channel.push((pushCntr))) {
                            ++pushCntr;
                            break;
                        }
                    }
                } else {
                    if (popCntr < values.size() && channel.pop(val)) {
                        ++popCntr;
                        break;
                    }
                }
                benchmark::DoNotOptimize(val);
            }
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.range(0) * state.iterations());
}

BENCHMARK(BM_RingBuffer_1Threads_random)->Arg(numItems)->UseRealTime();

RingBuffer<std::size_t, capacity> *channel_optional;
std::atomic_flag start_optional;
std::atomic_flag end_optional;

static void BM_RingBuffer_2Threads_optional(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::srand(seed);

    const bool producer = state.thread_index() & 1;

    RingBuffer<std::size_t, capacity> chan_opt;
    if (producer) {
        pin_thread(producerCpu);
        start_optional.wait(false, std::memory_order_acquire);
    } else {
        pin_thread(consumerCpu);
        channel_optional = &chan_opt;
        start_optional.test_and_set(std::memory_order_release);
        start_optional.notify_all();
    }

    std::vector<std::size_t> values(N);
    for (std::size_t i = 0; i < values.size(); ++i) { values[i] = static_cast<std::size_t>(std::rand()); }

    for (auto _ : state) {
        if (producer) {
            for (std::size_t i = 0U; i < values.size(); ++i) {
                if constexpr (randomValues) {
                    while (not channel_optional->push(values[i])) { }
                } else {
                    while (not channel_optional->push(i)) { }
                }
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

BENCHMARK(BM_RingBuffer_2Threads_optional)->Arg(numItems)->Threads(2)->UseRealTime();

RingBuffer<std::size_t, capacity> *channel_reference;
std::atomic_flag start_reference;
std::atomic_flag end_reference;

static void BM_RingBuffer_2Threads_reference(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::srand(seed);

    const bool producer = state.thread_index() & 1;

    RingBuffer<std::size_t, capacity> chan_ref;
    if (producer) {
        pin_thread(producerCpu);
        start_reference.wait(false, std::memory_order_acquire);
    } else {
        pin_thread(consumerCpu);
        channel_reference = &chan_ref;
        start_reference.test_and_set(std::memory_order_release);
        start_reference.notify_all();
    }

    std::vector<std::size_t> values(N);
    for (std::size_t i = 0; i < values.size(); ++i) { values[i] = static_cast<std::size_t>(std::rand()); }

    for (auto _ : state) {
        if (producer) {
            for (std::size_t i = 0U; i < values.size(); ++i) {
                if constexpr (randomValues) {
                    while (not channel_reference->push(values[i])) { }
                } else {
                    while (not channel_reference->push(i)) { }
                }
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

BENCHMARK(BM_RingBuffer_2Threads_reference)->Arg(numItems)->Threads(2)->UseRealTime();

BENCHMARK_MAIN();
