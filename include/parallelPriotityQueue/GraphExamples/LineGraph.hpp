#pragma once

#include "parallelPriotityQueue/QNetwork.hpp"

namespace spapq {

static constexpr QNetwork<2, 4> FULLY_CONNECTED_2_GRAPH(
    {0, 2, 4},
    {
        0, 1,
        1, 0
    }
);

} // end namespace spapq