#include "numbers.hpp"
#include "state_storage.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

namespace meandric {
namespace {

namespace fs = std::filesystem;
constexpr unsigned shard_count = 64;
constexpr std::size_t buffer_bytes = 1024 * 1024;
constexpr std::uint64_t job_records = 1024 * 1024;

struct SourceFile {
    fs::path path;
    int size;
    std::uint64_t records;
};

struct Job {
    std::size_t file;
    std::uint64_t first, count;
};

template<class Visit>
void read_records(const SourceFile& file, const Job& job,
    std::vector<char>& buffer, Visit&& visit) {
    const int key_bytes = code_size(file.size), record_bytes = key_bytes + sizeof(Count);
    std::ifstream input(file.path, std::ios::binary);
    input.seekg(job.first * record_bytes);
    for (auto remaining = job.count; remaining;) {
        const auto count = std::min<std::uint64_t>(remaining, buffer.size() / record_bytes);
        input.read(buffer.data(), count * record_bytes);
        for (std::size_t i = 0; i < count; ++i) {
            StateCode code{};
            Count ways;
            const char* record = buffer.data() + i * record_bytes;
            std::memcpy(code.data(), record, key_bytes);
            std::memcpy(&ways, record + key_bytes, sizeof ways);
            visit(code, ways);
        }
        remaining -= count;
    }
}

fs::path layer_file(const fs::path& scratch, int vertex, unsigned partition, int size) {
    return scratch / ("layer-" + std::to_string(vertex) + "-part-"
        + std::to_string(partition) + "-m" + std::to_string(size) + ".bin");
}

struct PartitionResult {
    Count completions = 0;
    std::uint64_t states = 0, bytes = 0;
    std::vector<SourceFile> files;
};

PartitionResult make_partition(const std::vector<SourceFile>& source,
    const std::vector<Job>& jobs, const fs::path& scratch, int vertex,
    unsigned partition, unsigned partitions, int open_limit, unsigned threads) {
    std::array<std::array<StateTable, shard_count>, max_open + 1> tables;
    std::array<std::mutex, shard_count> mutexes;
    for (int m = 3; m <= max_open; m += 2)
        for (auto& table : tables[m])
            table.set_size(m);

    struct Pending {
        StateCode code;
        std::uint64_t hash;
        Count ways;
        int size;
    };
    threads = std::min<std::size_t>(threads, jobs.size());
    std::atomic_size_t next_job = 0;
    std::vector<Count> completions(threads);
    {
        std::vector<std::jthread> workers;
        for (unsigned worker = 0; worker < threads; ++worker)
            workers.emplace_back([&, worker] {
                std::vector<char> buffer(buffer_bytes);
                std::array<std::vector<Pending>, shard_count> batches;
                for (auto& batch : batches)
                    batch.reserve(64);
                auto flush = [&](unsigned shard) {
                    std::lock_guard lock(mutexes[shard]);
                    for (const auto& item : batches[shard])
                        tables[item.size][shard].add(item.code, item.hash, item.ways);
                    batches[shard].clear();
                };
                Count finished = 0;
                for (std::size_t index; (index = next_job++) < jobs.size();) {
                    const auto& job = jobs[index];
                    const auto& file = source[job.file];
                    read_records(file, job, buffer, [&](const StateCode& code, Count ways) {
                        extend_frontier(decode(code, file.size), open_limit, [&](const Frontier& child) {
                            if (child.size == 1) {
                                // Every pass sees these completions; count them only in pass zero.
                                if (partition == 0 && is_completion(child))
                                    finished += ways;
                                return;
                            }
                            const auto child_code = encode(child);
                            const auto hash = hash_code(child_code, child.size);
                            if (((hash >> 32) & (partitions - 1)) != partition)
                                return;
                            const unsigned shard = (hash >> 48) & (shard_count - 1);
                            batches[shard].push_back({child_code, hash, ways, child.size});
                            if (batches[shard].size() == 64)
                                flush(shard);
                        });
                    });
                }
                for (unsigned shard = 0; shard < shard_count; ++shard)
                    flush(shard);
                completions[worker] = finished;
            });
    }

    PartitionResult result;
    for (auto count : completions)
        result.completions += count;
    std::vector<char> buffer(buffer_bytes);
    for (int m = 3; m <= max_open; m += 2) {
        std::uint64_t records = 0;
        for (const auto& table : tables[m])
            records += table.size();
        if (!records)
            continue;
        const auto path = layer_file(scratch, vertex, partition, m);
        const int key_bytes = code_size(m), record_bytes = key_bytes + sizeof(Count);
        std::ofstream output(path, std::ios::binary);
        std::size_t used = 0;
        for (const auto& table : tables[m])
            table.for_each([&](const StateCode& code, Count ways) {
                if (used + record_bytes > buffer.size()) {
                    output.write(buffer.data(), used);
                    used = 0;
                }
                std::memcpy(buffer.data() + used, code.data(), key_bytes);
                std::memcpy(buffer.data() + used + key_bytes, &ways, sizeof ways);
                used += record_bytes;
            });
        output.write(buffer.data(), used);
        result.files.push_back({path, m, records});
        result.states += records;
        result.bytes += records * record_bytes;
    }
    return result;
}

} // namespace

std::vector<Count> count_indecomposable(int max_n,
    const fs::path& scratch, unsigned partitions, unsigned threads) {
    fs::create_directories(scratch);
    Frontier initial;
    initial.size = 1;
    initial.initial_component_open = true;
    initial.mate[0] = path_start;
    const auto initial_path = layer_file(scratch, 0, 0, 1);
    {
        std::ofstream output(initial_path, std::ios::binary);
        const auto code = encode(initial);
        const Count ways = 1;
        output.write(reinterpret_cast<const char*>(code.data()), code_size(1));
        output.write(reinterpret_cast<const char*>(&ways), sizeof ways);
    }
    std::vector<SourceFile> source{{initial_path, 1, 1}};
    std::vector<Count> g(max_n + 1);
    for (int n = 1; n <= max_n; ++n) {
        const auto started = std::chrono::steady_clock::now();
        std::uint64_t source_states = 0;
        std::vector<Job> jobs;
        for (std::size_t i = 0; i < source.size(); ++i) {
            source_states += source[i].records;
            for (std::uint64_t first = 0; first < source[i].records; first += job_records)
                jobs.push_back({i, first, std::min(job_records, source[i].records - first)});
        }
        unsigned passes = partitions;
        if (!passes) {
            // At most ten children per state; aim for 400 million states per partition.
            passes = 1;
            while (passes < 32 && 10 * source_states > 400'000'000ULL * passes)
                passes *= 2;
        }
        std::vector<SourceFile> next;
        std::uint64_t states = 0, bytes = 0;
        for (unsigned p = 0; p < passes; ++p) {
            auto part = make_partition(source, jobs, scratch, n, p, passes,
                1 + 2 * (max_n - n), threads);
            g[n] += part.completions;
            states += part.states;
            bytes += part.bytes;
            next.insert(next.end(), part.files.begin(), part.files.end());
            if (passes > 1)
                std::cerr << "n=" << n << ", partition=" << p + 1 << '/' << passes << '\n';
        }
        for (const auto& file : source)
            fs::remove(file.path);
        source = std::move(next);
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        std::cerr << "n=" << n << ", g=" << g[n] << ", states=" << states
            << ", disk=" << bytes << ", time=" << seconds << " s\n";
    }
    for (const auto& file : source)
        fs::remove(file.path);
    return g;
}

} // namespace meandric
