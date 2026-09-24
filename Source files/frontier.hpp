#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

namespace meandric {

constexpr int max_open = 25;
constexpr std::uint8_t path_start = 255;

// Edges are ordered by left endpoint; same_start marks adjacent tied endpoints.
// A mate is the other open end of a partial path, or its connection to vertex 0.
struct Frontier {
    int size = 0;
    bool initial_component_open = false;
    std::array<std::uint8_t, max_open> page{};
    std::array<std::uint8_t, max_open> same_start{};
    std::array<std::uint8_t, max_open> mate{};
    std::array<std::uint8_t, max_open> component{};
};

struct Components {
    std::array<int, max_open> parent;

    explicit Components(int count) {
        std::iota(parent.begin(), parent.begin() + count, 0);
    }

    int find(int i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    }

    void join(int a, int b) { parent[find(b)] = find(a); }
};

template<class Emit>
void close_edges(const Frontier& state, std::array<int, 2> closing,
    const std::array<int, max_open>& start, int component_count, Emit&& emit) {
    const int closed = (closing[0] >= 0) + (closing[1] >= 0);
    if (closed == 2) {
        if (start[closing[0]] == start[closing[1]])
            return; // Repeated edge.
        if (state.mate[closing[0]] == closing[1])
            return; // Closed path cycle.
    }

    std::array<int, max_open> new_index, survivor{};
    new_index.fill(-1);
    int survivors = 0;
    for (int i = 0; i < state.size; ++i)
        if (i != closing[0] && i != closing[1]) {
            new_index[i] = survivors;
            survivor[survivors++] = i;
        }

    // A closing edge interlaces later-started surviving edges on the other page.
    Components components(component_count);
    for (int c = 0; c < closed; ++c) {
        const int a = closing[c];
        for (int b = a + 1; b < state.size; ++b)
            if (new_index[b] >= 0 && state.page[a] != state.page[b]
                && start[a] < start[b])
                components.join(state.component[a], state.component[b]);
    }

    std::array<bool, max_open> remains_open{};
    for (int s = 0; s < survivors; ++s)
        remains_open[components.find(state.component[survivor[s]])] = true;
    const int initial = state.initial_component_open ? components.find(0) : -1;

    // Fix the color of each finished noninitial component at its last-started edge.
    std::array<bool, max_open> finished{};
    for (int c = 0; c < closed; ++c) {
        const int root = components.find(state.component[closing[c]]);
        if (remains_open[root] || finished[root])
            continue;
        finished[root] = true;
        int last = closing[c];
        for (int d = c + 1; d < closed; ++d)
            if (components.find(state.component[closing[d]]) == root
                && start[closing[d]] > start[last])
                last = closing[d];
        if (root != initial && state.page[last] != 0)
            return;
    }

    const int opened = 2 - closed;
    Frontier child;
    child.size = survivors + opened;
    child.initial_component_open = initial >= 0 && remains_open[initial];
    for (int s = 0; s < survivors; ++s) {
        const int old = survivor[s];
        child.page[s] = state.page[old];
        child.same_start[s] = s > 0 && start[old] == start[survivor[s - 1]];
        const int mate = state.mate[old];
        if (mate == path_start)
            child.mate[s] = path_start;
        else if (new_index[mate] >= 0)
            child.mate[s] = new_index[mate];
    }

    // Closing exposes the former mate; opening supplies a new end. Join the two.
    std::array<int, 2> ends{};
    for (int i = 0; i < closed; ++i) {
        const int mate = state.mate[closing[i]];
        ends[i] = mate == path_start ? path_start : new_index[mate];
    }
    for (int i = 0; i < opened; ++i)
        ends[closed + i] = survivors + i;
    for (int i = 0; i < 2; ++i)
        if (ends[i] != path_start)
            child.mate[ends[i]] = ends[1 - i];

    // Canonical component labels follow first appearance, reserving 0 for initial.
    std::array<int, max_open> label;
    label.fill(-1);
    int labels = 0;
    if (child.initial_component_open)
        label[initial] = labels++;
    for (int s = 0; s < survivors; ++s) {
        const int root = components.find(state.component[survivor[s]]);
        if (label[root] < 0)
            label[root] = labels++;
        child.component[s] = label[root];
    }
    for (int i = 0; i < opened; ++i) {
        child.component[survivors + i] = labels++;
        child.same_start[survivors + i] = i == 1;
    }

    // Two edges opening together are unordered: their page choices are 00, 01, 11.
    for (int ones = 0; ones <= opened; ++ones) {
        for (int i = 0; i < opened; ++i)
            child.page[survivors + i] = i >= opened - ones;
        emit(child);
    }
}

template<class Emit>
void extend_frontier(const Frontier& state, int open_limit, Emit&& emit) {
    const int component_count = 1 + *std::max_element(
        state.component.begin(), state.component.begin() + state.size);
    std::array<int, max_open> start{};
    for (int i = 1; i < state.size; ++i)
        start[i] = start[i - 1] + !state.same_start[i];

    // Only the top two edges of either page can close at the next vertex.
    std::array<std::array<int, 2>, 2> top{{{-1, -1}, {-1, -1}}};
    for (int i = state.size - 1; i >= 0; --i) {
        auto& page = top[state.page[i]];
        if (page[0] < 0)
            page[0] = i;
        else if (page[1] < 0)
            page[1] = i;
    }

    auto close = [&](std::array<int, 2> edges) {
        const int closed = (edges[0] >= 0) + (edges[1] >= 0);
        if (state.size + 2 - 2 * closed <= std::min(open_limit, max_open))
            close_edges(state, edges, start, component_count, emit);
    };

    // With no closing edge, append a new path with two fresh components.
    if (state.size + 2 <= std::min(open_limit, max_open)) {
        Frontier child = state;
        const int first = state.size;
        child.size += 2;
        child.same_start[first] = 0;
        child.same_start[first + 1] = 1;
        child.mate[first] = first + 1;
        child.mate[first + 1] = first;
        child.component[first] = component_count;
        child.component[first + 1] = component_count + 1;
        for (int pattern = 0; pattern < 3; ++pattern) {
            child.page[first] = pattern == 2;
            child.page[first + 1] = pattern != 0;
            emit(child);
        }
    }
    for (int page = 0; page < 2; ++page)
        if (top[page][0] >= 0)
            close({top[page][0], -1});
    if (top[0][0] >= 0 && top[1][0] >= 0)
        close({top[0][0], top[1][0]});
    for (int page = 0; page < 2; ++page)
        if (top[page][1] >= 0)
            close({top[page][0], top[page][1]});
}

inline bool is_completion(const Frontier& state) {
    return state.size == 1 && state.mate[0] == path_start
        && (state.initial_component_open || state.page[0] == 0);
}

} // namespace meandric
