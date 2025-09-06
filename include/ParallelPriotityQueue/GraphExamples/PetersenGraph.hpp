#pragma once

#include "ParallelPriotityQueue/QNetwork.hpp"

namespace spapq {

static constexpr QNetwork<10, 30> PETERSEN_GRAPH(
    {0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30},
    {
        1, 5, 4,
        2, 6, 0,
        3, 7, 1,
        4, 8, 2,
        0, 9, 3,
        6, 0, 9,
        7, 1, 5,
        8, 2, 6,
        9, 3, 7,
        5, 4, 8
    }
);

} // end namespace spapq