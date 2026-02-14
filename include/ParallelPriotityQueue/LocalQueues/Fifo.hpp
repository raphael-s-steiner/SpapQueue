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

#include <deque>

namespace spapq {

/**
 * @brief A simple first-in-first-out queue 
 * 
 * @tparam T Value type
 */
template <typename T>
class Fifo {
  private:
    std::deque<T> deq_;

  public:
    using value_type = T;

    inline std::size_t size() const;
    inline bool empty() const;

    inline value_type top() const;
    inline void pop();

    inline void push(value_type obj);
    template <typename InputIt>
    inline void push(InputIt first, InputIt last);

    Fifo() = default;
    Fifo(const Fifo &other) = default;
    Fifo(Fifo &&other) = default;
    Fifo &operator=(const Fifo &other) = default;
    Fifo &operator=(Fifo &&other) = default;
    ~Fifo() = default;
};

template <typename T>
std::size_t Fifo<T>::size() const {
    return deq_.size();
}

template <typename T>
bool Fifo<T>::empty() const {
    return deq_.empty();
}

template <typename T>
T Fifo<T>::top() const {
    return deq_.front();
}

template <typename T>
void Fifo<T>::pop() {
    deq_.pop_front();
}

template <typename T>
void Fifo<T>::push(T obj) {
    deq_.emplace_back(obj);
}

template <typename T>
template <typename InputIt>
void Fifo<T>::push(InputIt first, InputIt last) {
    deq_.insert(deq_.end(), first, last);
}

}        // end namespace spapq
