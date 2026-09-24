#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace meandric::tangency {

constexpr int max_order = 24;

// Sorted factor codes: 1 is a bridge, 2*l+e is a cycle of length l and parity e.
// Positive loops (code 2) remain in the profile although their factor is 1.
using Profile = std::array<std::uint8_t, max_order>;
using Polynomial = std::array<std::uint32_t, max_order + 1>;

struct ArrayHash {
    template <typename T, std::size_t size>
    std::size_t operator()(const std::array<T, size>& values) const {
        std::uint64_t hash = 1469598103934665603ULL;
        for (auto value : values) {
            hash ^= value;
            hash *= 1099511628211ULL;
        }
        return static_cast<std::size_t>(hash);
    }
};

using ProfileSet = std::unordered_set<Profile, ArrayHash>;
using PolynomialSet = std::unordered_set<Polynomial, ArrayHash>;

struct Seed {
    int order, bridges, cycle_parity; // sum(length + parity) modulo 2 over cycles
    std::vector<std::uint8_t> cycles;
};

// Monomial supports through order 24.
inline bool allowed_monomial(int n, int k) {
    return n >= 5 && k >= 3 && k < n
        && !(n == 6 && k == 4) && !((n == 7 || n == 8) && k == 3)
        && !(n == 10 && k == 4);
}

inline Seed monomial_seed(int n, int k) {
    Seed seed{n, 0, (n + k) % 2, {}};
    seed.cycles.insert(seed.cycles.end(), n - k, 2);
    seed.cycles.insert(seed.cycles.end(), k, 3);
    return seed;
}

inline int cycle_code(int length, int parity) {
    return 2 * length + parity;
}

inline int factor_length(int code) {
    return code == 1 ? 1 : code / 2;
}

inline bool insert_seed(const Profile& outer, int position,
    const Seed& inner, Profile& result) {
    int code = outer[position], b = inner.bridges;
    int length = factor_length(code), parity = code % 2, count = 0;
    for (int i = 0; i < max_order && outer[i]; ++i)
        if (i != position)
            result[count++] = outer[i];

    if (code == 1) {
        for (int i = 0; i < b; ++i)
            result[count++] = 1;
    } else if (b > 0) {
        result[count++] = static_cast<std::uint8_t>(cycle_code(length - 1 + b,
            parity ^ ((b + 1 + inner.cycle_parity) % 2)));
    } else if (length > 1) {
        result[count++] = static_cast<std::uint8_t>(cycle_code(length - 1,
            parity ^ (1 ^ inner.cycle_parity)));
    } else if (parity != (1 ^ inner.cycle_parity)) {
        return false; // The terminal colors disagree at this loop.
    }

    for (auto cycle : inner.cycles)
        result[count++] = cycle;
    std::sort(result.begin(), result.begin() + count);
    return true;
}

inline void extend_profiles(std::vector<ProfileSet>& profiles, int n,
    const std::vector<Seed>& seeds) {
    const int limit = static_cast<int>(profiles.size()) - 1;
    for (const auto& profile : profiles[n])
        for (int i = 0; i < max_order && profile[i]; ++i) {
            if (i > 0 && profile[i] == profile[i - 1])
                continue;
            for (const auto& seed : seeds) {
                int next_order = n + seed.order - 1;
                if (next_order > limit)
                    continue;
                Profile result{};
                if (insert_seed(profile, i, seed, result))
                    profiles[next_order].insert(result);
            }
        }
}

inline Polynomial polynomial(const Profile& profile) {
    Polynomial result{};
    result[0] = 1;
    int degree = 0;
    for (auto code : profile) {
        if (!code) break;
        int length = factor_length(code);
        Polynomial factor{}, product{};
        if (code == 1) {
            factor[0] = factor[1] = 1;
        } else {
            std::uint64_t binomial = 1;
            for (int k = 0; k <= length; ++k) {
                if (k % 2 == code % 2)
                    factor[k] = static_cast<std::uint32_t>(binomial);
                binomial = binomial * (length - k) / (k + 1);
            }
        }
        for (int i = 0; i <= degree; ++i)
            if (result[i])
                for (int j = 0; j <= length; ++j)
                    product[i + j] += static_cast<std::uint32_t>(
                        std::uint64_t(result[i]) * factor[j]);
        result = product;
        degree += length;
    }
    return result;
}

} // namespace meandric::tangency
