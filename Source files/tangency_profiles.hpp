#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace meandric::tangency {

// A factorization is 2^(scale-1) times its bridge, loop and cycle factors.
// Profiles have scale=1. The all-zero value marks an unused hash-table slot.
// This 32-byte representation holds every profile and signature through n=45.
struct Factors {
    std::uint8_t scale = 0, bridges = 0, positive_loops = 0, negative_loops = 0;
    std::array<std::uint8_t, 28> cycles{}; // Sorted codes 2*length+parity; zero tail.

    bool operator==(const Factors&) const = default;

    void add_cycle(int code) {
        if (code == 2) { ++positive_loops; return; }
        if (code == 3) { ++negative_loops; return; }
        auto end = std::find(cycles.begin(), cycles.end(), 0);
        auto position = std::upper_bound(cycles.begin(), end, code);
        std::move_backward(position, end, end + 1);
        *position = static_cast<std::uint8_t>(code);
    }

    void remove_cycle(int code) {
        if (code == 2) { --positive_loops; return; }
        if (code == 3) { --negative_loops; return; }
        auto position = std::find(cycles.begin(), cycles.end(), code);
        std::move(position + 1, cycles.end(), position);
        cycles.back() = 0;
    }
};

// Linear probing avoids a separate allocation for each of millions of keys.
// Hash collisions are resolved by comparing the complete factorizations.
class FactorSet {
public:
    std::size_t size() const { return entries; }

    bool contains(const Factors& key) const {
        return slots[find_slot(key)].scale != 0;
    }

    bool insert(const Factors& key) {
        auto slot = find_slot(key);
        if (slots[slot].scale) return false;
        if (5 * (entries + 1) > 4 * slots.size()) {
            rehash(2 * slots.size());
            slot = find_slot(key);
        }
        slots[slot] = key;
        ++entries;
        return true;
    }

    void reserve(std::size_t expected) {
        std::size_t capacity = slots.size();
        while (5 * expected > 4 * capacity) capacity *= 2;
        if (capacity > slots.size()) rehash(capacity);
    }

private:
    std::vector<Factors> slots = std::vector<Factors>(8);
    std::size_t entries = 0;

    static std::uint64_t hash(const Factors& key) {
        std::uint64_t value = 0x9e3779b97f4a7c15ULL;
        const auto* bytes = reinterpret_cast<const unsigned char*>(&key);
        for (std::size_t offset = 0; offset < sizeof key; offset += 8) {
            std::uint64_t word;
            std::memcpy(&word, bytes + offset, sizeof word);
            value ^= word;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            value ^= value >> 31;
        }
        return value;
    }

    std::size_t find_slot(const Factors& key) const {
        const auto mask = slots.size() - 1;
        auto slot = static_cast<std::size_t>(hash(key)) & mask;
        while (slots[slot].scale && slots[slot] != key)
            slot = (slot + 1) & mask;
        return slot;
    }

    void rehash(std::size_t capacity) {
        auto previous = std::move(slots);
        slots = std::vector<Factors>(capacity);
        for (const auto& key : previous)
            if (key.scale) slots[find_slot(key)] = key;
    }
};

// Monomial generators (order, power), sufficient through order 45.
// Their realizability and the reduction of all other seeds are in ALGORITHM.md.
inline constexpr std::pair<int, int> monomial_seeds[] = {
    {5, 3}, {5, 4}, {7, 5}, {8, 4}, {8, 6}, {9, 3}, {11, 3}, {11, 5},
    {12, 4}, {13, 3}, {14, 4}, {15, 3}, {16, 4}, {17, 3}, {18, 4}, {19, 3},
    {20, 4}, {21, 3}, {22, 4}, {23, 3}, {24, 4}, {25, 3}, {26, 4}, {27, 3},
    {28, 4}, {29, 3}, {30, 4}, {31, 3}, {32, 4}, {33, 3}, {34, 4}, {35, 3},
    {36, 4}, {37, 3}, {38, 4}, {39, 3}, {40, 4}, {41, 3}, {42, 4}, {43, 3},
    {44, 4}, {45, 3}
};

// Enumerate each formal profile once: sorted cycles, then the three counts
// of length-one factors. Positive loops matter for subsequent insertions.
template<class Visit>
void all_profiles(int n, Visit visit) {
    Factors profile;
    profile.scale = 1;
    auto enumerate = [&](auto&& self, int remaining, int minimum, int position) -> void {
        for (int bridges = 0; bridges <= remaining; ++bridges) {
            profile.bridges = static_cast<std::uint8_t>(bridges);
            for (int loops = 0; loops <= remaining - bridges; ++loops) {
                profile.positive_loops = static_cast<std::uint8_t>(loops);
                profile.negative_loops = static_cast<std::uint8_t>(remaining - bridges - loops);
                visit(profile);
            }
        }
        profile.bridges = profile.positive_loops = profile.negative_loops = 0;
        for (int code = minimum; code <= 2 * remaining + 1; ++code) {
            profile.cycles[position] = static_cast<std::uint8_t>(code);
            self(self, remaining - code / 2, code, position + 1);
            profile.cycles[position] = 0;
        }
    };
    enumerate(enumerate, n, 4, 0);
}

// Earlier levels have been completely classified. A valid predecessor is
// reachable precisely when it is absent from that level's exception set.
inline bool has_predecessor(const Factors& child, int n,
    const std::vector<FactorSet>& excluded) {
    // Reverse 12: remove a bridge, or shorten a cycle and flip its parity.
    if (child.bridges >= 2) {
        auto parent = child;
        --parent.bridges;
        if (!excluded[n - 1].contains(parent)) return true;
    }
    for (std::size_t i = 0; i < child.cycles.size() && child.cycles[i]; ++i) {
        int code = child.cycles[i];
        if (i && code == child.cycles[i - 1]) continue;
        auto parent = child;
        parent.remove_cycle(code);
        parent.add_cycle(2 * (code / 2 - 1) + (1 ^ (code % 2)));
        if (!excluded[n - 1].contains(parent)) return true;
    }

    // After removing the inserted 21 or monomial, restore the selected
    // outer bridge, compatible loop, or cycle that the insertion shortened.
    auto restore_outer = [&](const Factors& base, int parity, int level) {
        auto parent = base;
        ++parent.bridges;
        if (!excluded[level].contains(parent)) return true;
        parent = base;
        parent.add_cycle(3 - parity);
        if (!excluded[level].contains(parent)) return true;

        auto lengthen = [&](int code) {
            auto candidate = base;
            candidate.remove_cycle(code);
            candidate.add_cycle(2 * (code / 2 + 1) + ((code % 2) ^ 1 ^ parity));
            return !excluded[level].contains(candidate);
        };
        if (base.positive_loops && lengthen(2)) return true;
        if (base.negative_loops && lengthen(3)) return true;
        for (std::size_t i = 0; i < base.cycles.size() && base.cycles[i]; ++i)
            if ((!i || base.cycles[i] != base.cycles[i - 1]) && lengthen(base.cycles[i]))
                return true;
        return false;
    };

    for (auto code : child.cycles) {
        if (!code || code > 5) break;
        if (code == 5) {
            auto base = child;
            base.remove_cycle(5);
            if (restore_outer(base, 1, n - 1)) return true;
            break;
        }
    }
    for (auto [order, power] : monomial_seeds) {
        if (order > n) break;
        if (child.positive_loops < order - power || child.negative_loops < power) continue;
        auto base = child;
        base.positive_loops -= order - power;
        base.negative_loops -= power;
        if (restore_outer(base, (order + power) % 2, n - order + 1)) return true;
    }
    return false;
}

// D_m = E_(m,m-1 mod 2), A_m = E_(m,m mod 2), D_1 = 1, A_1 = x.
// Repeatedly use D_(2m) = 2 D_m A_m to obtain the unique basis
// 2, 1+x, odd D_m (m>=3), and A_m (m>=1).
inline Factors signature(const Factors& profile) {
    Factors result;
    result.scale = 1;
    result.bridges = profile.bridges;
    result.negative_loops = profile.negative_loops;
    for (auto code : profile.cycles) {
        if (!code) break;
        int length = code / 2;
        if (code % 2 == length % 2) {
            result.add_cycle(code);
        } else {
            while (length % 2 == 0) {
                length /= 2;
                ++result.scale;
                result.add_cycle(2 * length + length % 2);
            }
            if (length > 1) result.add_cycle(2 * length);
        }
    }
    return result;
}

inline bool constant_one(const Factors& signature) {
    if (signature.scale != 1 || signature.negative_loops) return false;
    for (auto code : signature.cycles) {
        if (!code) break;
        if (code % 2) return false;
    }
    return true;
}

} // namespace meandric::tangency
