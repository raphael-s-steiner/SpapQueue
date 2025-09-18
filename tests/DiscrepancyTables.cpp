#include <gtest/gtest.h>

#include "Discrepancy/TableGenerator.hpp"

using namespace spapq;

static constexpr std::array<std::size_t, 5U> testArr1 = {1U, 1U, 1U, 1U, 1U};
static constexpr std::array<std::size_t, 3U> testArr2 = {1U, 2U, 3U};
static constexpr std::array<std::size_t, 6U> testArr3 = {8U, 6U, 4U, 3U, 2U, 1U};
static constexpr std::array<std::size_t, 6U> testArr4 = {11U, 4U, 8U, 13U, 3U, 7U};
static constexpr std::array<std::size_t, 4U> testArr5 = {2U, 4U, 8U, 2U};
static constexpr std::array<std::size_t, 5U> testArr6 = {6U, 12U, 15U, 39U, 45U};

template<std::size_t N, std::size_t tableSize>
constexpr bool validTable(const std::array<std::size_t, tableSize> &table, const std::array<std::size_t, N> &frequencies) {
    std::size_t sum = std::accumulate(frequencies.cbegin(), frequencies.cend(), static_cast<std::size_t>(0U));
    if (sum % tableSize != 0U) {
        return false;
    }
    const std::size_t ratio = sum / tableSize;

    std::array<std::size_t, N> occurrences;
    for (std::size_t &val : occurrences) {
        val = 0U;
    }

    for (const std::size_t &symb : table) {
        if (symb >= frequencies.size()) return false;
        ++occurrences[symb];
    }

    for (std::size_t i = 0U; i < frequencies.size(); ++i) {
        if (frequencies[i] % occurrences[i] != 0U) return false;
        if (frequencies[i] != ratio * occurrences[i]) return false;
    }

    return true;
}

template<std::size_t N, std::size_t tableSize>
constexpr bool satisfiesDiscrepancyInequality(const std::array<std::size_t, tableSize> &table, const std::array<std::size_t, N> &frequencies) {
    assert(validTable(table, frequencies));

    const std::size_t ratio = std::accumulate(frequencies.cbegin(), frequencies.cend(), static_cast<std::size_t>(0U)) / tableSize;
    
    std::array<std::size_t, N> numAllocs;
    for (std::size_t &val : numAllocs) {
        val = 0U;
    }

    for (std::size_t i = 0U; i < table.size(); ++i) {
        ++numAllocs[table[i]];

        for (std::size_t s = 0U; s < frequencies.size(); ++s) {
            std::size_t expectedOccurrences = (i * (frequencies[s] / ratio)) / tableSize;
            std::size_t remainder = (i * (frequencies[s] / ratio)) % tableSize;

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

    constexpr auto table5 = EARLIEST_DEADLINE_FIRST_TABLE(testArr5);
    EXPECT_TRUE( validTable(table5, testArr5) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table5, testArr5) );

    constexpr auto table6 = EARLIEST_DEADLINE_FIRST_TABLE(testArr6);
    EXPECT_TRUE( validTable(table6, testArr6) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table6, testArr6) );
}

TEST(DiscrepancyTablesTest, ReducedEarliestDeadlineFirst) {
    constexpr auto table1 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr1);
    EXPECT_TRUE( validTable(table1, testArr1) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table1, testArr1) );
    
    constexpr auto table2 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr2);
    EXPECT_TRUE( validTable(table2, testArr2) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table2, testArr2) );
    
    constexpr auto table3 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr3);
    EXPECT_TRUE( validTable(table3, testArr3) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table3, testArr3) );
    
    constexpr auto table4 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr4);
    EXPECT_TRUE( validTable(table4, testArr4) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table4, testArr4) );

    constexpr auto table5 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr5);
    EXPECT_TRUE( validTable(table5, testArr5) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table5, testArr5) );

    constexpr auto table6 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr6);
    EXPECT_TRUE( validTable(table6, testArr6) );
    EXPECT_TRUE( satisfiesDiscrepancyInequality(table6, testArr6) );
}