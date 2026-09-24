#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace meandric {

using Count = std::uint64_t;

struct TangencyNumbers {
    Count N = 0, O = 0;
};

// Entries 1 through max_n, for 1 <= max_n <= 24.
// Partitions: 0 selects automatically; otherwise a power of two. Threads: positive.
std::vector<Count> count_indecomposable(int max_n,
    const std::filesystem::path& scratch, unsigned partitions, unsigned threads);
std::vector<TangencyNumbers> count_tangencies(int max_n);

} // namespace meandric
