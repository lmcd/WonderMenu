/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <functional>

template <class T, std::size_t N, class Compare = std::less<T>>
class StaticSet {
    std::array<T, N> data_{};   // contiguous, inline storage
    std::size_t      size_ = 0; // number of live elements

public:
    bool insert(const T& v) {
        auto end = data_.begin() + size_;
        auto it = std::lower_bound(data_.begin(), end, v, Compare{});

        if (it != end && !Compare{}(v, *it)) {
            return false; // already present
        }
        
        if (size_ == N) {
            return false; // full (or assert/throw)
        }
        
        std::move_backward(it, end, end + 1);              // shift right
        *it = v;

        ++size_;

        return true;
    }

    bool erase(const T& v) {
        auto end = data_.begin() + size_;
        auto it = std::lower_bound(data_.begin(), end, v, Compare{});

        if (it == end || Compare{}(v, *it)) {
            return false;
        }

        std::move(it + 1, end, it); // shift left

        --size_;

        return true;
    }

    void clear() {
        size_ = 0;
    }

    bool contains(const T& v) const {
        auto end = data_.begin() + size_;
        auto it  = std::lower_bound(data_.begin(), end, v, Compare{});

        return it != end && !Compare{}(v, *it);
    }

    std::size_t size() const {
        return size_;
    }

    constexpr std::size_t capacity() const {
        return N;
    }

    const T* begin() const {
        return data_.data();
    }

    const T* end() const {
        return data_.data() + size_;
    }
};