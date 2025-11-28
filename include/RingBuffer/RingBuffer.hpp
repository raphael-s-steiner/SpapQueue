#pragma once

#include <array>
#include <atomic>
#include <limits>
#include <optional>
#include <type_traits>

#include "Configuration/config.hpp"

namespace spapq {

/**
 * @brief A single-producer single consumer first-in-first-out queue implemented as a ring buffer on
 * the stack
 *
 * @tparam T Data type
 * @tparam N Size of ring buffer
 */
template <typename T, std::size_t N>
class RingBuffer {
  private:
    alignas(CACHE_LINE_SIZE) std::array<T, N> data_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> tailCounter_{N};
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> headCounter_{N};
    alignas(CACHE_LINE_SIZE) std::size_t cachedTailCounter_{N};
    alignas(CACHE_LINE_SIZE) std::size_t cachedHeadCounter_{N};
    char padding_[CACHE_LINE_SIZE - sizeof(std::size_t)];

  protected:
    inline std::size_t getTailPosition() const noexcept {
        return tailCounter_.load(std::memory_order_relaxed) % N;
    };

    inline std::size_t getHeadPosition() const noexcept {
        return headCounter_.load(std::memory_order_relaxed) % N;
    };

    inline void advanceTail(std::size_t n = 1U) noexcept {
        tailCounter_.fetch_add(n, std::memory_order_release);
    };

    inline void advanceHead(std::size_t n = 1U) noexcept {
        headCounter_.fetch_add(n, std::memory_order_release);
    };

  public:
    RingBuffer() = default;
    RingBuffer(const RingBuffer &other) = default;
    RingBuffer(RingBuffer &&other) = default;
    RingBuffer &operator=(const RingBuffer &other) = default;
    RingBuffer &operator=(RingBuffer &&other) = default;
    ~RingBuffer() = default;

    inline constexpr std::size_t getCapacity() const noexcept { return N; };

    inline bool isEmpty() const noexcept {
        return tailCounter_.load(std::memory_order_relaxed) == headCounter_.load(std::memory_order_acquire);
    };

    inline bool isFull() const noexcept {
        return tailCounter_.load(std::memory_order_acquire) + N
               == headCounter_.load(std::memory_order_relaxed);
    };

    inline std::size_t occupancy() const noexcept {
        return headCounter_.load(std::memory_order_acquire) - tailCounter_.load(std::memory_order_acquire);
    };

    inline std::optional<T> pop() noexcept;
    [[nodiscard("Pop may fail when queue is empty")]] inline bool pop(T &out) noexcept;
    [[nodiscard("Push may fail when queue is full")]] inline bool push(const T &value) noexcept;
    [[nodiscard("Push may fail when queue is full")]] inline bool push(T &&value) noexcept;
    template <class InputIt>
    [[nodiscard("Push may fail when queue is full")]] inline bool push(InputIt first, InputIt last) noexcept;

    // assertions
    static_assert(N > 0U, "No trivial RingBuffers allowed!");
    static_assert(N < std::numeric_limits<std::size_t>::max(),
                  "Needed to differentiate empty from full RingBuffer.");
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "Want atomic to be lock free.");

    // overflow protection
    // can be commented out if number of inserts will be less than the maximum value of std::size_t
    // to relax this condition, one can have tailCounter_ and headCounter_ be modulo 2N assuming 2N
    // <= max value of std::size_t
    static_assert(sizeof(std::size_t) >= 8 || (((std::numeric_limits<std::size_t>::max() - N + 1U) % N == 0U)),
                  "Modulo operations need to be consistent or number of operations need to be "
                  "smaller than max value of std::size_t!");
};

template <typename T, std::size_t N>
inline std::optional<T> RingBuffer<T, N>::pop() noexcept {
    std::optional<T> result(std::nullopt);
    const std::size_t tail = tailCounter_.load(std::memory_order_relaxed);
    if ((cachedHeadCounter_ != tail)
        || ((cachedHeadCounter_ = headCounter_.load(std::memory_order_acquire)) != tail)) {
        const std::size_t pos = tail % N;
        result = std::move(data_[pos]);
        data_[pos].~T();
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
        out = std::move(data_[pos]);
        data_[pos].~T();
        advanceTail();
    }
    return hasData;
}

template <typename T, std::size_t N>
inline bool RingBuffer<T, N>::push(const T &value) noexcept {
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
inline bool RingBuffer<T, N>::push(T &&value) noexcept {
    const std::size_t head = headCounter_.load(std::memory_order_relaxed);
    const std::size_t headLoopAround = head - N;
    const bool nonFull
        = (cachedTailCounter_ != headLoopAround)
          || ((cachedTailCounter_ = tailCounter_.load(std::memory_order_acquire)) != headLoopAround);
    if (nonFull) {
        data_[head % N] = std::move(value);
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
        while (first != last) {
            data_[head % N] = *first;
            ++head;
            ++first;
        }
        advanceHead(numElements);
    }
    return enoughSpace;
}

}    // end namespace spapq
