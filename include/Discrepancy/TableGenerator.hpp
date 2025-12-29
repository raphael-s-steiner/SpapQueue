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
#include <cassert>
#include <limits>
#include <numeric>

namespace spapq {
namespace tables {

/**
 * @brief Divides each entry of the array by the gcd of the whole array. If all entries are zero, it returns
 * the original array.
 *
 */
template <std::size_t N>
constexpr std::array<std::size_t, N> reducedIntegerArray(const std::array<std::size_t, N> &arr) {
    std::size_t commonGCD = 0U;
    for (const std::size_t &val : arr) { commonGCD = std::gcd(commonGCD, val); }

    std::array<std::size_t, N> reducedArr = arr;
    if (commonGCD > 0U) {
        for (std::size_t &val : reducedArr) { val /= commonGCD; }
    }

    return reducedArr;
};

/**
 * @brief Sums all elements of the array.
 *
 */
template <std::size_t N>
constexpr std::size_t sumArray(const std::array<std::size_t, N> &arr) {
    return std::accumulate(arr.cbegin(), arr.cend(), static_cast<std::size_t>(0U));
};

/**
 * @brief Finds the smallest integer n such that ((frequency * n) / tableSize) >= lbVal through binary search.
 *
 * @param lower Lower bound on the search interval. Must either be 0 or satisfy ((frequency * (lower - 1U)) /
 * tableSize) < lbVal.
 * @param upper Upper bound of the search interval. Must satisfy ((frequency * upper) / tableSize) >= lbVal.
 */
constexpr std::size_t findEarliestdeadline(
    std::size_t lower, std::size_t upper, std::size_t frequency, std::size_t tableSize, std::size_t lbVal) {
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

/**
 * @brief Compute a so-called table, which is a series (array) A such that for N = 0,...,tableSize and for
 * s=0,...,M-1, we have that |#{n in [0,N[ | A[n] == s} - frequencies[s] * N / tableSize| is bounded by 1 (in
 * infinite precision).
 *
 * @tparam M Size of the array frequencies.
 * @tparam tableSize Sum of the entries in frequencies, which the size of the output table.
 * @param frequencies The value frequencies[i] marks the number of occurences of i inside the table
 */
template <std::size_t M, std::size_t tableSize>
constexpr std::array<std::size_t, tableSize> earliestDeadlineFirstTable(
    const std::array<std::size_t, M> &frequencies) {
    static_assert(tableSize <= (std::numeric_limits<std::size_t>::max() >> ((sizeof(std::size_t) * 4U) + 1U)),
                  "May overflow if this condition is not met!");
    assert(tableSize
           == std::accumulate(frequencies.cbegin(), frequencies.cend(), static_cast<std::size_t>(0U)));
    assert(std::all_of(
        frequencies.cbegin(), frequencies.cend(), [](const std::size_t &freq) { return (freq != 0U); }));

    std::array<std::size_t, tableSize> table;

    std::array<std::size_t, M> numAllocs;
    for (std::size_t i = 0U; i < numAllocs.size(); ++i) { numAllocs[i] = 0U; }

    for (std::size_t i = 0U; i < table.size(); ++i) {
        constexpr std::size_t limit = table.size() * 2U;
        std::size_t u = limit;

        for (std::size_t s = 0U; s < numAllocs.size(); ++s) {
            if (numAllocs[s] != ((i * frequencies[s]) / tableSize)) { continue; }
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

/**
 * @brief Extends an array to a larger size and fills in the new elements with the maximum value of
 * std::size_t.
 *
 * @tparam after Size of the array after the extension.
 * @tparam before Size of the array before the extension.
 * @param table Array.
 */
template <std::size_t after, std::size_t before>
constexpr std::array<std::size_t, after> extendTable(const std::array<std::size_t, before> &table) {
    static_assert(after >= before);

    std::array<std::size_t, after> longTable;
    for (std::size_t i = 0U; i < table.size(); ++i) { longTable[i] = table[i]; }
    for (std::size_t i = table.size(); i < longTable.size(); ++i) {
        longTable[i] = std::numeric_limits<std::size_t>::max();
    }

    return longTable;
}

/**
 * @brief Helper macro to compute the earliest deadline first table.
 *
 * @see earliestDeadlineFirstTable
 */
#define EARLIEST_DEADLINE_FIRST_TABLE(frequencies)                                                        \
    (spapq::tables::earliestDeadlineFirstTable<frequencies.size(),                                        \
                                               spapq::tables::sumArray<frequencies.size()>(frequencies)>( \
        frequencies))

/**
 * @brief Helper macro to compute the earliest deadline first table based on the reduced frequencies.
 *
 * @see earliestDeadlineFirstTable
 */
#define REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(frequencies)                                               \
    (EARLIEST_DEADLINE_FIRST_TABLE(spapq::tables::reducedIntegerArray<frequencies.size()>(frequencies)))

}        // end namespace tables
}        // end namespace spapq
