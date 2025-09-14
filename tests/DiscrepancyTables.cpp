#include <gtest/gtest.h>

#include "Discrepancy/TableGenerator.hpp"

using namespace spapq;

static constexpr std::array<std::size_t, 5U> testArr1 = {1U, 1U, 1U, 1U, 1U};
static constexpr std::array<std::size_t, 3U> testArr2 = {1U, 2U, 3U};
static constexpr std::array<std::size_t, 6U> testArr3 = {8U, 6U, 4U, 3U, 2U, 1U};
static constexpr std::array<std::size_t, 6U> testArr4 = {11U, 4U, 8U, 13U, 3U, 7U};

template<std::size_t N, std::size_t tableSize>
constexpr bool validTable(const std::array<std::size_t, tableSize> &table, const std::array<std::size_t, N> &frequencies) {
    if (tableSize != std::accumulate(frequencies.cbegin(), frequencies.cend(), static_cast<std::size_t>(0U))) {
        return false;
    }

    std::array<std::size_t, N> occurrences;
    for (std::size_t &val : occurrences) {
        val = 0U;
    }

    for (const std::size_t &symb : table) {
        if (symb >= frequencies.size()) return false;
        ++occurrences[symb];
    }

    for (std::size_t i = 0U; i < frequencies.size(); ++i) {
        if (occurrences[i] != frequencies[i]) return false;
    }

    return true;
}

template<std::size_t N, std::size_t tableSize>
constexpr bool satisfiesDiscrepancyInequality(const std::array<std::size_t, tableSize> &table, const std::array<std::size_t, N> &frequencies) {
    assert(validTable(table, frequencies));
    
    std::array<std::size_t, N> numAllocs;
    for (std::size_t &val : numAllocs) {
        val = 0U;
    }

    for (std::size_t i = 0U; i < table.size(); ++i) {
        ++numAllocs[table[i]];

        for (std::size_t s = 0U; s < frequencies.size(); ++s) {
            std::size_t expectedOccurrences = (i * frequencies[s]) / tableSize;
            std::size_t remainder = (i * frequencies[s]) % tableSize;

            if (remainder == 0U) {
                if ((numAllocs[s] != expectedOccurrences) & (numAllocs[s] != expectedOccurrences - 1U) & (numAllocs[s] != expectedOccurrences + 1U)) {
                    return false;
                }
            } else {
                if ((numAllocs[s] != expectedOccurrences) & (numAllocs[s] != expectedOccurrences + 1U)) {
                    return false;
                }
            }
        }
    }

    return true;
}

TEST(DiscrepancyTablesTest, EarliestDeadlineFirst) {
    constexpr auto table1 = EARLIEST_DEADLINE_FIRST_TABLE(testArr1);
    EXPECT_TRUE( validTable(table1, testArr1) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table1, testArr1) );
    
    constexpr auto table2 = EARLIEST_DEADLINE_FIRST_TABLE(testArr2);
    EXPECT_TRUE( validTable(table2, testArr2) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table2, testArr2) );
    
    constexpr auto table3 = EARLIEST_DEADLINE_FIRST_TABLE(testArr3);
    EXPECT_TRUE( validTable(table3, testArr3) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table3, testArr3) );
    
    constexpr auto table4 = EARLIEST_DEADLINE_FIRST_TABLE(testArr4);
    EXPECT_TRUE( validTable(table4, testArr4) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table4, testArr4) );
}


