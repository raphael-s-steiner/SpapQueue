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

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <iterator>
#include <limits>
#include <optional>
#include <type_traits>

#include "Configuration/config.hpp"

namespace spapq {

/**
 * @brief A single-producer single consumer first-in-first-out queue implemented as a ring buffer on
 * the stack.
 *
 * @tparam T Data type.
 * @tparam N Size or capacity.
 */
template <typename T, std::size_t N>
class alignas(CACHE_LINE_SIZE) RingBuffer {
  private:
    alignas(CACHE_LINE_SIZE) std::array<T, N> data_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> tailCounter_{N};
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> headCounter_{N};
    alignas(CACHE_LINE_SIZE) std::size_t cachedTailCounter_{N};
    alignas(CACHE_LINE_SIZE) std::size_t cachedHeadCounter_{N};
    char padding_[CACHE_LINE_SIZE - sizeof(std::size_t)];

    inline std::size_t getTailPosition() const noexcept;
    inline std::size_t getHeadPosition() const noexcept;

    inline void advanceTail(std::size_t n = 1U) noexcept;
    inline void advanceHead(std::size_t n = 1U) noexcept;

  public:
    RingBuffer() = default;
    RingBuffer(const RingBuffer &other) = delete;
    RingBuffer(RingBuffer &&other) = delete;
    RingBuffer &operator=(const RingBuffer &other) = delete;
    RingBuffer &operator=(RingBuffer &&other) = delete;
    ~RingBuffer() = default;

    inline constexpr std::size_t capacity() const noexcept;

    inline bool empty() const noexcept;
    inline bool full() const noexcept;
    inline std::size_t occupancy() const noexcept;

    inline std::optional<T> pop() noexcept;
    [[nodiscard("Pop may fail when queue is empty.\n")]] inline bool pop(T &out) noexcept;
    [[nodiscard("Push may fail when queue is full.\n")]] inline bool push(const T value) noexcept;
    template <class InputIt>
    [[nodiscard("Push may fail when queue is full.\n")]] inline bool push(InputIt first,
                                                                          InputIt last) noexcept;

    // assertions
    static_assert(N > 0U, "No trivial RingBuffers allowed!\n");
    static_assert(N < std::numeric_limits<std::size_t>::max(),
                  "Needed to differentiate empty from full RingBuffer.\n");
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "Want atomic to be lock free.\n");
    static_assert(std::is_nothrow_default_constructible_v<T>);
    static_assert(std::is_nothrow_copy_constructible_v<T>);

    // overflow protection
    // can be commented out if number of inserts will be less than the maximum value of std::size_t
    // to relax this condition, one can have tailCounter_ and headCounter_ be modulo 2N assuming 2N
    // <= max value of std::size_t
    static_assert(sizeof(std::size_t) >= 8 || (((std::numeric_limits<std::size_t>::max() - N + 1U) % N == 0U)),
                  "Modulo operations need to be consistent or number of operations need to be "
                  "smaller than max value of std::size_t!\n");
};

// Implementation details

template <typename T, std::size_t N>
inline std::size_t RingBuffer<T, N>::getTailPosition() const noexcept {
    return tailCounter_.load(std::memory_order_relaxed) % N;
};

template <typename T, std::size_t N>
inline std::size_t RingBuffer<T, N>::getHeadPosition() const noexcept {
    return headCounter_.load(std::memory_order_relaxed) % N;
};

template <typename T, std::size_t N>
inline void RingBuffer<T, N>::advanceTail(std::size_t n) noexcept {
    tailCounter_.fetch_add(n, std::memory_order_release);
};

template <typename T, std::size_t N>
inline void RingBuffer<T, N>::advanceHead(std::size_t n) noexcept {
    headCounter_.fetch_add(n, std::memory_order_release);
};

/**
 * @brief The number of elements the Ringbuffer can maximally hold.
 *
 */
template <typename T, std::size_t N>
inline constexpr std::size_t RingBuffer<T, N>::capacity() const noexcept {
    return N;
};

/**
 * @brief Checks whether the Ringbuffer is empty. If used to check whether one can pop from the Ringbuffer it
 * is better (more performant) to just use pop and check if it succeeded.
 *
 */
template <typename T, std::size_t N>
inline bool RingBuffer<T, N>::empty() const noexcept {
    return tailCounter_.load(std::memory_order_relaxed) == headCounter_.load(std::memory_order_acquire);
};

/**
 * @brief Checks whether the Ringbuffer is full. If used to check whether one can push to the Ringbuffer it
 * is better (more performant) to just use push and check if it succeeded.
 *
 */
template <typename T, std::size_t N>
inline bool RingBuffer<T, N>::full() const noexcept {
    return tailCounter_.load(std::memory_order_acquire) + N == headCounter_.load(std::memory_order_relaxed);
};

/**
 * @brief Returns the number of elements currently in the Ringbuffer. If used to check whether one can push
 * to/pop from the Ringbuffer it is better (more performant) to just use push/pop and check if it succeeded.
 *
 */
template <typename T, std::size_t N>
inline std::size_t RingBuffer<T, N>::occupancy() const noexcept {
    return headCounter_.load(std::memory_order_acquire) - tailCounter_.load(std::memory_order_acquire);
};

template <typename T, std::size_t N>
inline std::optional<T> RingBuffer<T, N>::pop() noexcept {
    std::optional<T> result(std::nullopt);
    const std::size_t tail = tailCounter_.load(std::memory_order_relaxed);
    if ((cachedHeadCounter_ != tail)
        || ((cachedHeadCounter_ = headCounter_.load(std::memory_order_acquire)) != tail)) {
        const std::size_t pos = tail % N;
        result = data_[pos];
        advanceTail();
    }
    return result;
}

template <typename T, std::size_t N>
inline bool RingBuffer<T, N>::pop(T &out) noexcept {
    const std::size_t tail = tailCounter_.load(std::memory_order_relaxed);
    const bool hasData = (cachedHeadCounter_ != tail)
                         || ((cachedHeadCounter_ = headCounter_.load(std::memory_order_acquire)) != tail);
    if (hasData) {
        const std::size_t pos = tail % N;
        out = data_[pos];
        advanceTail();
    }
    return hasData;
}

template <typename T, std::size_t N>
inline bool RingBuffer<T, N>::push(const T value) noexcept {
    const std::size_t head = headCounter_.load(std::memory_order_relaxed);
    const std::size_t headLoopAround = head - N;
    const bool nonFull
        = (cachedTailCounter_ != headLoopAround)
          || ((cachedTailCounter_ = tailCounter_.load(std::memory_order_acquire)) != headLoopAround);
    if (nonFull) {
        data_[head % N] = value;
        advanceHead();
    }
    return nonFull;
}

template <typename T, std::size_t N>
template <class InputIt>
inline bool RingBuffer<T, N>::push(InputIt first, InputIt last) noexcept {
    const std::size_t numElements = static_cast<std::size_t>(std::distance(first, last));
    std::size_t head = headCounter_.load(std::memory_order_relaxed);

    bool enoughSpace;
    if constexpr ((sizeof(std::size_t) >= 8) && (N <= ((std::numeric_limits<std::size_t>::max() / 2) + 1U))) {
        const std::size_t diff = head - N + numElements;
        enoughSpace = (cachedTailCounter_ >= diff)
                      || ((cachedTailCounter_ = tailCounter_.load(std::memory_order_acquire)) >= diff);
    } else {
        const std::size_t diff = N - numElements;
        enoughSpace
            = ((head - cachedTailCounter_ <= diff)
               || ((head - (cachedTailCounter_ = tailCounter_.load(std::memory_order_acquire))) <= diff));
    }

    if (enoughSpace) {
        const std::size_t headIndx = head % N;
        
        const std::size_t numElementsFirstPush = std::min(N - headIndx, numElements);
        const std::size_t numElementsSecondPush = numElements - numElementsFirstPush;
        
        auto dataIt = data_.begin();
        std::advance(dataIt, headIndx);
        std::copy_n(first, numElementsFirstPush, dataIt);

        std::advance(first, numElementsFirstPush);
        std::copy_n(first, numElementsSecondPush, data_.begin());

        advanceHead(numElements);
    }
    return enoughSpace;
}

}        // end namespace spapq
