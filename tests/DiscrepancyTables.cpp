#include <gtest/gtest.h>

#include "Discrepancy/QNetworkTables.hpp"
#include "Discrepancy/TableGenerator.hpp"

using namespace spapq;

static constexpr std::array<std::size_t, 5U> testArr1 = {1U, 1U, 1U, 1U, 1U};
static constexpr std::array<std::size_t, 3U> testArr2 = {1U, 2U, 3U};
static constexpr std::array<std::size_t, 6U> testArr3 = {8U, 6U, 4U, 3U, 2U, 1U};
static constexpr std::array<std::size_t, 6U> testArr4 = {11U, 4U, 8U, 13U, 3U, 7U};
static constexpr std::array<std::size_t, 4U> testArr5 = {2U, 4U, 8U, 2U};
static constexpr std::array<std::size_t, 5U> testArr6 = {6U, 12U, 15U, 39U, 45U};

template <std::size_t N, std::size_t tableSize>
constexpr bool validTable(const std::array<std::size_t, tableSize> &table,
                          const std::array<std::size_t, N> &frequencies) {
    std::size_t sum = std::accumulate(frequencies.cbegin(), frequencies.cend(), static_cast<std::size_t>(0U));
    if (sum % tableSize != 0U) { return false; }
    const std::size_t ratio = sum / tableSize;

    std::array<std::size_t, N> occurrences;
    for (std::size_t &val : occurrences) { val = 0U; }

    for (const std::size_t &symb : table) {
        if (symb >= frequencies.size()) { return false; }
        ++occurrences[symb];
    }

    for (std::size_t i = 0U; i < frequencies.size(); ++i) {
        if (frequencies[i] % occurrences[i] != 0U) { return false; }
        if (frequencies[i] != ratio * occurrences[i]) { return false; }
    }

    return true;
}

template <std::size_t N, std::size_t tableSize>
constexpr bool satisfiesDiscrepancyInequality(const std::array<std::size_t, tableSize> &table,
                                              const std::array<std::size_t, N> &frequencies) {
    assert(validTable(table, frequencies));

    const std::size_t ratio
        = std::accumulate(frequencies.cbegin(), frequencies.cend(), static_cast<std::size_t>(0U)) / tableSize;

    std::array<std::size_t, N> numAllocs;
    for (std::size_t &val : numAllocs) { val = 0U; }

    for (std::size_t i = 0U; i < table.size(); ++i) {
        ++numAllocs[table[i]];

        for (std::size_t s = 0U; s < frequencies.size(); ++s) {
            std::size_t expectedOccurrences = (i * (frequencies[s] / ratio)) / tableSize;
            std::size_t remainder = (i * (frequencies[s] / ratio)) % tableSize;

            if (remainder == 0U) {
                if ((numAllocs[s] != expectedOccurrences)
                    & (numAllocs[s] != expectedOccurrences - 1U)
                    & (numAllocs[s] != expectedOccurrences + 1U)) {
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
    EXPECT_TRUE(validTable(table1, testArr1));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table1, testArr1));

    constexpr auto table2 = EARLIEST_DEADLINE_FIRST_TABLE(testArr2);
    EXPECT_TRUE(validTable(table2, testArr2));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table2, testArr2));

    constexpr auto table3 = EARLIEST_DEADLINE_FIRST_TABLE(testArr3);
    EXPECT_TRUE(validTable(table3, testArr3));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table3, testArr3));

    constexpr auto table4 = EARLIEST_DEADLINE_FIRST_TABLE(testArr4);
    EXPECT_TRUE(validTable(table4, testArr4));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table4, testArr4));

    constexpr auto table5 = EARLIEST_DEADLINE_FIRST_TABLE(testArr5);
    EXPECT_TRUE(validTable(table5, testArr5));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table5, testArr5));

    constexpr auto table6 = EARLIEST_DEADLINE_FIRST_TABLE(testArr6);
    EXPECT_TRUE(validTable(table6, testArr6));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table6, testArr6));
}

TEST(DiscrepancyTablesTest, ReducedEarliestDeadlineFirst) {
    constexpr auto table1 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr1);
    EXPECT_TRUE(validTable(table1, testArr1));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table1, testArr1));

    constexpr auto table2 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr2);
    EXPECT_TRUE(validTable(table2, testArr2));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table2, testArr2));

    constexpr auto table3 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr3);
    EXPECT_TRUE(validTable(table3, testArr3));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table3, testArr3));

    constexpr auto table4 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr4);
    EXPECT_TRUE(validTable(table4, testArr4));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table4, testArr4));

    constexpr auto table5 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr5);
    EXPECT_TRUE(validTable(table5, testArr5));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table5, testArr5));

    constexpr auto table6 = REDUCED_EARLIEST_DEADLINE_FIRST_TABLE(testArr6);
    EXPECT_TRUE(validTable(table6, testArr6));
    EXPECT_TRUE(satisfiesDiscrepancyInequality(table6, testArr6));
}

TEST(DiscrepancyTablesTest, QNetworkTableFrequency1) {
    constexpr auto graph = QNetwork<2, 4>({0, 2, 4}, {0, 1, 1, 0}, {2, 1, 1, 2}, {1, 2, 1, 2});

    constexpr auto tableFreq0 = tables::qNetworkTableFrequencies<2, 4, 2>(graph, 0);
    constexpr auto tableFreq1 = tables::qNetworkTableFrequencies<2, 4, 2>(graph, 1);

    EXPECT_EQ(tableFreq0.size(), 2U);
    EXPECT_EQ(tableFreq1.size(), 2U);

    EXPECT_EQ(tableFreq0[0], 4U);
    EXPECT_EQ(tableFreq0[1], 1U);

    EXPECT_EQ(tableFreq1[0], 1U);
    EXPECT_EQ(tableFreq1[1], 1U);

    std::size_t worker = 0U;
    for (std::size_t i = graph.vertexPointer_[worker]; i < graph.vertexPointer_[worker + 1]; ++i) {
        for (std::size_t j = graph.vertexPointer_[worker] + 1; j < graph.vertexPointer_[worker + 1]; ++j) {
            EXPECT_EQ(
                tableFreq0[i - graph.vertexPointer_[worker]] * graph.batchSize_[i] * graph.multiplicities_[j],
                tableFreq0[j - graph.vertexPointer_[worker]] * graph.batchSize_[j] * graph.multiplicities_[i]);
        }
    }

    worker = 1U;
    for (std::size_t i = graph.vertexPointer_[worker]; i < graph.vertexPointer_[worker + 1]; ++i) {
        for (std::size_t j = graph.vertexPointer_[worker] + 1; j < graph.vertexPointer_[worker + 1]; ++j) {
            EXPECT_EQ(
                tableFreq1[i - graph.vertexPointer_[worker]] * graph.batchSize_[i] * graph.multiplicities_[j],
                tableFreq1[j - graph.vertexPointer_[worker]] * graph.batchSize_[j] * graph.multiplicities_[i]);
        }
    }

    const auto table0 = tables::qNetworkTable<2, 4, 2, 5>(graph, 0U);
    const auto table1 = tables::qNetworkTable<2, 4, 2, 2>(graph, 1U);

    for (const std::size_t &val : table0) { EXPECT_TRUE(val == 0U || val == 1U); }
    for (const std::size_t &val : table1) { EXPECT_TRUE(val == 2U || val == 3U); }
}

TEST(DiscrepancyTablesTest, QNetworkTableFrequency2) {
    constexpr auto graph = QNetwork<4, 8>(
        {0, 2, 4, 6, 8}, {0, 1, 1, 2, 2, 3, 3, 0}, {2, 1, 1, 2, 3, 2, 3, 2}, {1, 2, 1, 2, 2, 3, 6, 9});

    for (std::size_t worker = 0U; worker < graph.numWorkers_; ++worker) {
        const auto tableFreq = tables::qNetworkTableFrequencies<4, 8, 2>(graph, worker);
        for (std::size_t i = graph.vertexPointer_[worker]; i < graph.vertexPointer_[worker + 1]; ++i) {
            for (std::size_t j = graph.vertexPointer_[worker] + 1; j < graph.vertexPointer_[worker + 1]; ++j) {
                EXPECT_EQ(tableFreq[i - graph.vertexPointer_[worker]]
                              * graph.batchSize_[i]
                              * graph.multiplicities_[j],
                          tableFreq[j - graph.vertexPointer_[worker]]
                              * graph.batchSize_[j]
                              * graph.multiplicities_[i]);
            }
        }
    }

    std::array<bool, 8> foundChannel = {false, false, false, false, false, false, false, false};
    const auto table0 = tables::qNetworkTable<4, 8, 2, 5>(graph, 0U);
    for (const std::size_t &val : table0) {
        EXPECT_TRUE(val == 0U || val == 1U);
        foundChannel[val] = true;
    }

    const auto table1 = tables::qNetworkTable<4, 8, 2, 2>(graph, 1U);
    for (const std::size_t &val : table1) {
        EXPECT_TRUE(val == 2U || val == 3U);
        foundChannel[val] = true;
    }

    const auto table2 = tables::qNetworkTable<4, 8, 2, 13>(graph, 2U);
    for (const std::size_t &val : table2) {
        EXPECT_TRUE(val == 4U || val == 5U);
        foundChannel[val] = true;
    }

    const auto table3 = tables::qNetworkTable<4, 8, 2, 13>(graph, 3U);
    for (const std::size_t &val : table3) {
        EXPECT_TRUE(val == 6U || val == 7U);
        foundChannel[val] = true;
    }

    for (const bool val : foundChannel) { EXPECT_TRUE(val); }
}

TEST(DiscrepancyTablesTest, TableExpansion) {
    constexpr std::size_t extendedSize = 17U;

    static constexpr std::array<std::size_t, extendedSize> extArr1
        = tables::extendTable<extendedSize>(testArr1);
    for (std::size_t i = 0U; i < extArr1.size(); ++i) {
        if (i < testArr1.size()) {
            EXPECT_EQ(extArr1[i], testArr1[i]);
        } else {
            EXPECT_EQ(extArr1[i], std::numeric_limits<std::size_t>::max());
        }
    }

    static constexpr std::array<std::size_t, extendedSize> extArr2
        = tables::extendTable<extendedSize>(testArr2);
    for (std::size_t i = 0U; i < extArr2.size(); ++i) {
        if (i < testArr2.size()) {
            EXPECT_EQ(extArr2[i], testArr2[i]);
        } else {
            EXPECT_EQ(extArr2[i], std::numeric_limits<std::size_t>::max());
        }
    }

    static constexpr std::array<std::size_t, extendedSize> extArr3
        = tables::extendTable<extendedSize>(testArr3);
    for (std::size_t i = 0U; i < extArr3.size(); ++i) {
        if (i < testArr3.size()) {
            EXPECT_EQ(extArr3[i], testArr3[i]);
        } else {
            EXPECT_EQ(extArr3[i], std::numeric_limits<std::size_t>::max());
        }
    }

    static constexpr std::array<std::size_t, extendedSize> extArr4
        = tables::extendTable<extendedSize>(testArr4);
    for (std::size_t i = 0U; i < extArr4.size(); ++i) {
        if (i < testArr4.size()) {
            EXPECT_EQ(extArr4[i], testArr4[i]);
        } else {
            EXPECT_EQ(extArr4[i], std::numeric_limits<std::size_t>::max());
        }
    }

    static constexpr std::array<std::size_t, extendedSize> extArr5
        = tables::extendTable<extendedSize>(testArr5);
    for (std::size_t i = 0U; i < extArr5.size(); ++i) {
        if (i < testArr5.size()) {
            EXPECT_EQ(extArr5[i], testArr5[i]);
        } else {
            EXPECT_EQ(extArr5[i], std::numeric_limits<std::size_t>::max());
        }
    }

    static constexpr std::array<std::size_t, extendedSize> extArr6
        = tables::extendTable<extendedSize>(testArr6);
    for (std::size_t i = 0U; i < extArr6.size(); ++i) {
        if (i < testArr6.size()) {
            EXPECT_EQ(extArr6[i], testArr6[i]);
        } else {
            EXPECT_EQ(extArr6[i], std::numeric_limits<std::size_t>::max());
        }
    }
}
