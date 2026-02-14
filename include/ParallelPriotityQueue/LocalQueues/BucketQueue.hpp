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

#include <map>

namespace spapq {

template <typename T, typename Compare = std::less<T>>
class BucketQueue {
  private:
    std::size_t size_{0U};
    std::map<T, std::size_t, Compare> cntrs_;

  public:
    using value_type = T;

    inline std::size_t size() const;
    inline bool empty() const;

    inline value_type top() const;
    inline void pop();

    inline void push(value_type obj);

    BucketQueue() = default;
    BucketQueue(const BucketQueue &other) = default;
    BucketQueue(BucketQueue &&other) = default;
    BucketQueue &operator=(const BucketQueue &other) = default;
    BucketQueue &operator=(BucketQueue &&other) = default;
    ~BucketQueue() = default;
};

template <typename T, class Compare>
std::size_t BucketQueue<T, Compare>::size() const {
    return size_;
}

template <typename T, class Compare>
bool BucketQueue<T, Compare>::empty() const {
    return cntrs_.empty();
}

template <typename T, class Compare>
T BucketQueue<T, Compare>::top() const {
    return cntrs_.cbegin()->first;
}

template <typename T, class Compare>
void BucketQueue<T, Compare>::pop() {
    --size_;
    if (--(cntrs_.begin()->second) == 0U) { cntrs_.erase(cntrs_.begin()); };
}

template <typename T, class Compare>
void BucketQueue<T, Compare>::push(T obj) {
    ++size_;
    auto it = cntrs_.find(obj);
    if (it == cntrs_.end()) {
        cntrs_.emplace(obj, 1U);
    } else {
        ++(it->second);
    }
}

}        // end namespace spapq
