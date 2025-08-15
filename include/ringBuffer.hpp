#pragma once

#include <array>
#include <limits>

#include "config.hpp"

namespace parPrioQ
{


template<typename T, std::size_t N>
class RingBuffer {
    private:
        alignas(CACHE_LINE_SIZE) volatile std::size_t tailCounter_{0};
        alignas(CACHE_LINE_SIZE) volatile std::size_t headCounter_{0};
        alignas(CACHE_LINE_SIZE) std::array<T, N> data_;

    protected:
        inline std::size_t getTailPosition() const noexcept { return tailCounter_ % N; };
        inline std::size_t getHeadPosition() const noexcept { return headCounter_ % N; };

        inline void advanceTail(std::size_t n = 1U) noexcept { tailCounter_ += n; };
        inline void advanceHead(std::size_t n = 1U) noexcept { headCounter_ += n; };

    public:
        RingBuffer() = default;
        RingBuffer(const RingBuffer &other) = default;
        RingBuffer(RingBuffer &&other) = default;
        RingBuffer& operator=(const RingBuffer &other) = default;
        RingBuffer& operator=(RingBuffer &&other) = default;
        ~RingBuffer() = default;

        inline constexpr std::size_t getCapacity() const noexcept { return N; };

        inline bool isEmpty() const noexcept { return tailCounter_ == headCounter_; };
        inline bool isFull() const noexcept { return tailCounter_ + N == headCounter_; };

        inline std::size_t occupancy() const noexcept { return headCounter_ - tailCounter_; };

        inline const T& front() const noexcept { return data_[getTailPosition()]; };
        
        inline bool pop() noexcept;
        inline bool push(const T &value) noexcept;

        template<class InputIt>
        inline bool push(InputIt first, InputIt last) noexcept;

        // assertions
        static_assert(N > 0U && "No trivial RingBuffers allowed!");
        static_assert(N < std::numeric_limits<std::size_t>::max() && "Needed to differentiate empty from full RingBuffer.");

        // overflow protection
        // can be commented out if number of inserts will be less than the maximum value of std::size_t
        // to relax this condition, one can have tailCounter_ and headCounter_ be modulo 2N assuming 2N <= max value of std::size_t
        static_assert((std::numeric_limits<std::size_t>::max() - N + 1U) % N == 0U && "Modulo operations need to be consistent!");
};

template<typename T, std::size_t N>
inline bool RingBuffer<T, N>::pop() noexcept {
    bool nonEmpty = !isEmpty();
    if (nonEmpty) {
        advanceTail();
    }
    return nonEmpty;
};

template<typename T, std::size_t N>
inline bool RingBuffer<T, N>::push(const T &value) noexcept {
    bool nonFull = !isFull();
    if (nonFull) {
        data_[getHeadPosition()] = T;
        advanceHead();
    }
    return nonFull;
};

template<typename T, std::size_t N>
template<class InputIt>
inline bool RingBuffer<T, N>::push<InputIt>(InputIt first, InputIt last) noexcept {
    std::size_t numElements = static_cast<std::size_t>( std::distance(first, last) );
    std::size_t occ = occupancy();

    bool enoughSpace = ((numElements + occ) <= N);
    if (enoughSpace) {
        while (first != last) {
            data_[getHeadPosition()] = *first;
            advanceHead();
            ++first;
        }
    }
    return enoughSpace;
};


} // end namespace parPrioQ