/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

// Mixing hash: std::hash<int> is the identity, which clusters badly
// with power-of-two masking. This is the MurmurHash3 64-bit finalizer.
struct IntHash {
    std::size_t operator()(int k) const noexcept {
        std::uint64_t x = static_cast<std::uint32_t>(k);
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return static_cast<std::size_t>(x);
    }
};

template <class K, class V, std::size_t N,
          class Hash = std::hash<K>, class Eq = std::equal_to<K>>
class StaticMap {
    static_assert(N >= 2 && (N & (N - 1)) == 0, "N must be a power of two");
    static constexpr std::size_t mask = N - 1;

    std::array<K, N>    keys_{};
    std::array<V, N>    vals_{};
    std::array<bool, N> used_{};
    std::size_t         size_ = 0;
    V                   sink_{};  // written to when a full map is subscripted

    static std::size_t home(const K& k) {
        return Hash{}(k) & mask;
    }

    // Index of k if present, otherwise the empty slot where it would go.
    // Terminates because at least one slot is always kept empty.
    std::size_t probe(const K& k) const {
        std::size_t i = home(k);

        while (used_[i] && !Eq{}(keys_[i], k)) {
            i = (i + 1) & mask;
        }

        return i;
    }

public:
    bool insert(const K& k, const V& v) {
        std::size_t i = probe(k);
        if (used_[i]) {
            return false;        // key already present
        }

        if (size_ + 1 >= N) {
            return false;  // full: keep one slot empty
        }

        keys_[i] = k;
        vals_[i] = v;
        used_[i] = true;

        ++size_;

        return true;
    }

    // Inserts k, or overwrites the value if k is already present.
    // Returns false only when the map is full.
    bool insertOrAssign(const K& k, const V& v) {
        std::size_t i = probe(k);

        if (used_[i]) {
            vals_[i] = v;
            return true;
        }

        if (size_ + 1 >= N) {
            return false;  // full: keep one slot empty
        }

        keys_[i] = k;
        vals_[i] = v;
        used_[i] = true;

        ++size_;

        return true;
    }

    // Reference to the value for k, inserting a default-constructed value if
    // k is not present. When the map is full the reference is to a scratch
    // slot, so the write is discarded rather than corrupting another entry.
    V& operator[](const K& k) {
        std::size_t i = probe(k);

        if (used_[i]) {
            return vals_[i];
        }

        if (size_ + 1 >= N) {
            sink_ = V{};
            return sink_;  // full: keep one slot empty
        }

        keys_[i] = k;
        vals_[i] = V{};
        used_[i] = true;

        ++size_;

        return vals_[i];
    }

    void clear() {
        used_.fill(false);
        size_ = 0;
    }

    V* find(const K& k) {
        std::size_t i = probe(k);
        return used_[i] ? &vals_[i] : nullptr;
    }

    const V* find(const K& k) const {
        std::size_t i = probe(k);
        return used_[i] ? &vals_[i] : nullptr;
    }

    bool erase(const K& k) {
        std::size_t i = probe(k);

        if (!used_[i]) {
            return false;
        }
        
        // Backward-shift deletion: pull later entries of the probe chain
        // into the hole, so no tombstones are needed.
        for (std::size_t j = i;;) {
            j = (j + 1) & mask;

            if (!used_[j]) {
                break;
            }

            std::size_t h = home(keys_[j]);

            if (((j - h) & mask) >= ((j - i) & mask)) {  // j may move to i
                keys_[i] = std::move(keys_[j]);
                vals_[i] = std::move(vals_[j]);
                i = j;
            }
        }

        used_[i] = false;
        --size_;

        return true;
    }

    std::size_t size() const {
        return size_;
    }

    static constexpr std::size_t capacity() {
        return N - 1;
    }
};