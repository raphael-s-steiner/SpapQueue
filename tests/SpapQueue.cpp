#include "ParallelPriotityQueue/SpapQueue.hpp"

#include <gtest/gtest.h>

#include "ParallelPriotityQueue/GraphExamples/FullyConnectedGraph.hpp"

using namespace spapq;

// template <typename T>
// class Test {
//     template <std::size_t N>
//     struct WorkerCollectiveHelper;
// };

// template <typename T>
// template <std::size_t N>
// struct Test<T>::WorkerCollectiveHelper {
//     template <typename... Args>
//     using partialType
//         = std::conditional_t<N == 0,
//                              std::tuple<Args...>,
//                              typename WorkerCollectiveHelper<N - 1>::template partialType<std::unique_ptr<int>,
//                                                                                           Args...>>;
// };


TEST(SpapQueueTest, Constructors) {
    constexpr QNetwork<1, 2> netw({0, 2}, {0, 0});
    EXPECT_TRUE(netw.hasHomogeneousInPorts());
    EXPECT_TRUE(netw.hasHomogeneousOutPorts());
    EXPECT_TRUE(netw.hasHomogeneousPorts());
    EXPECT_EQ(netw.maxPortNum(), 2U);
}
