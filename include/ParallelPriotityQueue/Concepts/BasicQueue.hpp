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

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace spapq {

template <typename T>
concept BasicQueue = requires { typename std::remove_cvref_t<T>::value_type; }
                     && requires (std::remove_cvref_t<T> queue, std::remove_cvref_t<T>::value_type obj) {
                            { queue.size() } -> std::convertible_to<std::size_t>;
                            { queue.empty() } -> std::convertible_to<bool>;
                            queue.push(obj);
                            {
                                queue.top()
                            } -> std::convertible_to<typename std::remove_cvref_t<T>::value_type>;
                            queue.pop();
                        };

}        // end namespace spapq
