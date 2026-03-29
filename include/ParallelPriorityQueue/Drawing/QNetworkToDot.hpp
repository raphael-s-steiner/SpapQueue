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

#pragma once

#include <iomanip>
#include <ios>
#include <fstream>
#include <string>

#include "ParallelPriorityQueue/Drawing/WorkLoadDistribution.hpp"
#include "ParallelPriorityQueue/QNetwork.hpp"

namespace spapq {

template <std::size_t workers, std::size_t channels>
void globalQNetworkInfo(const QNetwork<workers, channels> &netw, std::ostream &os) {
    os << "    subgraph Info{\n";
    os << "    NetworkInfo [\n";
    os << "        label=<\n";
    os << "            <table border='3' cellborder='1' cellspacing='4'>\n";
    os << "                <tr><td align=\"center\">QNetwork Information</td></tr>\n";
    os << "                <tr><td align=\"center\"><table border='0' cellborder='0' cellspacing='1'>\n";
    os << "                    <tr><td align=\"left\">Number of Workers</td><td align=\"right\">" << netw.numWorkers_ << "</td></tr>\n";
    os << "                    <tr><td align=\"left\">Number of Channels</td><td align=\"right\">" << netw.numChannels_ << "</td></tr>\n";
    os << "                    <tr><td align=\"left\">Buffer Size</td><td align=\"right\">" << netw.channelBufferSize_ << "</td></tr>\n";
    os << "                    <tr><td align=\"left\">Enqueue Frequency</td><td align=\"right\">" << netw.enqueueFrequency_ << "</td></tr>\n";
    os << "                    <tr><td align=\"left\">Max. Push Attempts</td><td align=\"right\">" << netw.maxPushAttempts_ << "</td></tr>\n";
    os << "                </table></td></tr>\n";
    os << "            </table>\n";
    os << "        >\n";
    os << "    ];\n";
    os << "    }\n\n";
}

template <std::size_t workers, std::size_t channels>
void QNetworkWorkerInfo(const QNetwork<workers, channels> &netw, const std::array<double, workers> &loads, const std::size_t worker, std::ostream &os) {
    os << "    Worker" << worker << " [\n";
    os << "        label=<\n";
    os << "            <table border='2' cellborder='1' cellspacing='2'>\n";
    os << "                <tr><td align=\"center\" colspan='2'><b>Worker " << worker << "</b></td></tr>\n";
    os << "                <tr><td align=\"left\">Core</td><td align=\"right\">" << netw.logicalCore_[worker] << "</td></tr>\n";
    os << "                <tr><td align=\"left\">Load</td><td align=\"right\">" << loads[worker] << "</td></tr>\n";
    os << "            </table>\n";
    os << "        >\n";
    os << "    ];\n\n";
}

template <std::size_t workers, std::size_t channels>
void QNetworkChannelInfo(const QNetwork<workers, channels> &netw, const std::size_t edge, std::ostream &os) {
    const std::size_t source = netw.source(edge);
    const std::size_t target = netw.target(edge);

    os << "    Worker" << source << " -> Worker" << target << " [\n";
    os << "        label=<\n";
    os << "            <table border='0' cellborder='1' cellspacing='0'>\n";
    os << "                <tr><td align=\"left\">Mult.</td><td align=\"right\">" << netw.multiplicities_[edge] << "</td></tr>\n";
    os << "                <tr><td align=\"left\">Batch</td><td align=\"right\">" << netw.batchSize_[edge] << "</td></tr>\n";
    os << "            </table>\n";
    os << "        >\n";
    os << "    ];\n";
}

template <std::size_t workers, std::size_t channels>
void QNetworkToDot(const QNetwork<workers, channels> &netw, std::ostream &os) {
    if (!os.is_open()) {
        std::cerr << "Failed to open file to write QNetwork dot!" << std::endl;
        return;
    }

    const std::array<double, workers> loads = workLoadDistribution(netw);

    // Header
    os << std::fixed << std::setprecision(2);
    os << "digraph QNetwork{\n";
    os << "    node [shape=plaintext;]\n";
    os << "    edge [shape=plaintext; arrowhead=\"vee\"; fontsize=\"8\";]\n\n";

    //  Global Network Info
    globalQNetworkInfo(netw, os);

    //  Actual Network
    os << "    subgraph cluster_network{\n";
    os << "    label=\"QNetwork\";\n";
    os << "    margin=\"30\";\n\n";

    // Workers (Vertices)
    for (std::size_t worker = 0U; worker < workers; ++worker) {
        QNetworkWorkerInfo(netw, loads, worker, os);
    }

    // Channels (Edges)
    for (std::size_t worker = 0U; worker < workers; ++worker) {
        for (std::size_t edge = netw.vertexPointer_[worker]; edge < netw.vertexPointer_[worker + 1U]; ++edge) {
            QNetworkChannelInfo(netw, edge, os);
        }
    }

    os << "    }\n";
    os << "}\n";
}

template <std::size_t workers, std::size_t channels>
void QNetworkToDot(const QNetwork<workers, channels> &netw, const std::string &filename) {
    std::ofstream os(filename);
    WriteTxt(netw, os);
}

}        // namespace spapq
