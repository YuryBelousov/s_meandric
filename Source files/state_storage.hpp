#pragma once

#include "frontier.hpp"
#include "numbers.hpp"

#include <cstring>
#include <mutex>

namespace meandric {

using StateCode = std::array<std::uint64_t, 5>;

inline int code_size(int size) { return (1 + 12 * size + 7) / 8; }

// One initial-component flag, then page, tied start, mate and component per edge.
inline StateCode encode(const Frontier& state) {
    StateCode code{};
    code[0] = state.initial_component_open;
    for (int i = 0; i < state.size; ++i) {
        const auto mate = state.mate[i] == path_start ? 31 : state.mate[i];
        const std::uint64_t edge = state.page[i] | (state.same_start[i] << 1)
            | (mate << 2) | (state.component[i] << 7);
        const int bit = 1 + 12 * i, word = bit / 64, shift = bit % 64;
        code[word] |= edge << shift;
        if (shift > 52)
            code[word + 1] |= edge >> (64 - shift);
    }
    return code;
}

inline Frontier decode(const StateCode& code, int size) {
    Frontier state;
    state.size = size;
    state.initial_component_open = code[0] & 1;
    for (int i = 0; i < size; ++i) {
        const int bit = 1 + 12 * i, word = bit / 64, shift = bit % 64;
        std::uint64_t edge = code[word] >> shift;
        if (shift > 52)
            edge |= code[word + 1] << (64 - shift);
        state.page[i] = edge & 1;
        state.same_start[i] = (edge >> 1) & 1;
        const int mate = (edge >> 2) & 31;
        state.mate[i] = mate == 31 ? path_start : mate;
        state.component[i] = (edge >> 7) & 31;
    }
    return state;
}

inline std::uint64_t hash_code(const StateCode& code, int size) {
    auto mix = [](std::uint64_t value) {
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    };
    std::uint64_t hash = 0x9e3779b97f4a7c15ULL ^ size;
    for (int word = 0; word < (code_size(size) + 7) / 8; ++word)
        hash = mix(hash ^ code[word]);
    return hash;
}

// Tables contain one frontier size. Byte-packed keys and separate counts avoid
// padding and per-state allocations; a zero count marks an unused slot.
class StateTable {
public:
    explicit StateTable(int size = 0) { set_size(size); }

    void set_size(int size) {
        frontier_size = size;
        key_bytes = code_size(size);
    }

    void add(const StateCode& code, std::uint64_t hash, Count ways) {
        if (values.empty())
            rehash(8);
        for (;;) {
            const auto mask = values.size() - 1;
            auto slot = static_cast<std::size_t>(hash) & mask;
            while (values[slot]) {
                if (std::memcmp(key_at(slot), code.data(), key_bytes) == 0) {
                    values[slot] += ways;
                    return;
                }
                slot = (slot + 1) & mask;
            }
            if (4 * (entries + 1) <= 3 * values.size()) {
                std::memcpy(key_at(slot), code.data(), key_bytes);
                values[slot] = ways;
                ++entries;
                return;
            }
            rehash(2 * values.size());
        }
    }

    template<class Visit>
    void for_each(Visit&& visit) const {
        for (std::size_t slot = 0; slot < values.size(); ++slot)
            if (values[slot]) {
                StateCode code{};
                std::memcpy(code.data(), key_at(slot), key_bytes);
                visit(code, values[slot]);
            }
    }

    std::size_t size() const { return entries; }

private:
    std::vector<std::uint8_t> keys;
    std::vector<Count> values;
    std::size_t entries = 0;
    int frontier_size = 0, key_bytes = 0;
    inline static std::mutex growth_mutex;

    std::uint8_t* key_at(std::size_t slot) { return keys.data() + slot * key_bytes; }
    const std::uint8_t* key_at(std::size_t slot) const {
        return keys.data() + slot * key_bytes;
    }

    void insert_unique(const StateCode& code, Count ways) {
        const auto mask = values.size() - 1;
        auto slot = static_cast<std::size_t>(hash_code(code, frontier_size)) & mask;
        while (values[slot])
            slot = (slot + 1) & mask;
        std::memcpy(key_at(slot), code.data(), key_bytes);
        values[slot] = ways;
        ++entries;
    }

    void rehash(std::size_t capacity) {
        // Only one shard keeps both its old and new allocations at a time.
        std::lock_guard lock(growth_mutex);
        auto old_keys = std::move(keys);
        auto old_values = std::move(values);
        keys.resize(capacity * key_bytes);
        values.assign(capacity, 0);
        entries = 0;
        for (std::size_t slot = 0; slot < old_values.size(); ++slot)
            if (old_values[slot]) {
                StateCode code{};
                std::memcpy(code.data(), old_keys.data() + slot * key_bytes, key_bytes);
                insert_unique(code, old_values[slot]);
            }
    }
};

} // namespace meandric
