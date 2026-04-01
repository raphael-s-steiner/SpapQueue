/*
Copyright 2026 Raphael S. Steiner

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

#include <iostream>

#include "ParallelPriorityQueue/QNetwork/Drawing/QNetworkToDot.hpp"
#include "ParallelPriorityQueue/QNetwork/GraphExamples/FullyConnectedGraph.hpp"
#include "ParallelPriorityQueue/QNetwork/GraphExamples/LineGraph.hpp"
#include "ParallelPriorityQueue/QNetwork/GraphExamples/PetersenGraph.hpp"

using namespace spapq;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage:\n"
                  << "  " << argv[0] << " <output file>\n";
        return 1;
    }

    std::ofstream os(argv[1]);

    constexpr auto netw = LINE_GRAPH(FULLY_CONNECTED_GRAPH<3U>());

    QNetworkToDot(netw, os);
    return 0;
}
