#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <numeric>

namespace spapq {
namespace tables {

template<std::size_t N>
constexpr std::array<std::size_t, N> reducedIntegerArray(const std::array<std::size_t, N> arr) {
    std::size_t commonGCD = 0U;
    for (const std::size_t &val : arr) {
        commonGCD = std::gcd(commonGCD, val);
    }

    assert(commonGCD > 0U);
    std::array<std::size_t, N> reducedArr;
    for (std::size_t i = 0U; i < arr.size(); ++i) {
        reducedArr[i] = arr[i] / commonGCD;
    }

    return reducedArr;
};

template<std::size_t N>
constexpr std::size_t sumArray(const std::array<std::size_t, N> arr) {
    return std::accumulate(arr.cbegin(), arr.cend(), static_cast<std::size_t>(0U));
};

constexpr std::size_t findEarliestdeadline(std::size_t lower, std::size_t upper, std::size_t frequency, std::size_t tableSize, std::size_t lbVal) {
    assert(((frequency * upper) / tableSize) >= lbVal);
    assert((lower == 0U) || (((frequency * (lower - 1U)) / tableSize) < lbVal));

    while (lower < upper) {
        const std::size_t mid = lower + ((upper - lower) / 2);

        if (((frequency * mid) / tableSize) >= lbVal) {
            upper = mid;
        } else {
            lower = mid + 1U;
        }
    }

    return upper;
}

template<std::size_t M, std::size_t tableSize>
constexpr std::array<std::size_t, tableSize> EarliestDeadlineFirstTable(const std::array<std::size_t, M> frequencies) {
    static_assert(tableSize <= (std::numeric_limits<std::size_t>::max() >> ((sizeof(std::size_t) * 4U) + 1U)), "May overflow if this condition is not met!");
    assert(tableSize == std::accumulate(frequencies.cbegin(), frequencies.cend(), static_cast<std::size_t>(0U)));
    assert(std::all_of(frequencies.cbegin(), frequencies.cend(), [](const std::size_t &freq) { return (freq != 0U); } ));

    std::array<std::size_t, tableSize> table;

    std::array<std::size_t, M> numAllocs;
    for (std::size_t i = 0U; i < numAllocs.size(); ++i) {
        numAllocs[i] = 0U;
    }

    for (std::size_t i = 0U; i < table.size(); ++i) {
        constexpr std::size_t limit = table.size() * 2U;
        std::size_t u = limit;

        for (std::size_t s = 0U; s < numAllocs.size(); ++s) {
            if (numAllocs[s] != ((i * frequencies[s]) / tableSize)) continue;
            if (u == limit || ((frequencies[s] * u) / tableSize) >= numAllocs[s] + 1) {
                std::size_t u_prime = findEarliestdeadline(i, u, frequencies[s], tableSize, numAllocs[s] + 1);
                if (u_prime <= u) {
                    table[i] = s;
                    u = u_prime;
                }
            }
        }
        ++numAllocs[table[i]];
    }

    return table;
};

#define EARLIEST_DEADLINE_FIRST_TABLE(frequencies) (spapq::tables::EarliestDeadlineFirstTable<frequencies.size(), spapq::tables::sumArray<frequencies.size()>(frequencies)>(frequencies))
#define REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(frequencies) (EARLIEST_DEADLINE_FIRST_TABLE(spapq::tables::reducedIntegerArray<frequencies.size()>(frequencies)))

} // end namespace tables
} // end namespace spapq