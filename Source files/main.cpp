#include "numbers.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    const int max_n = argc > 1 ? std::stoi(argv[1]) : 8;
    const auto tangencies = meandric::count_tangencies(max_n);
    if (argc > 2 && std::string(argv[2]) == "--polynomials") {
        std::cout << "n,N_n,O_n\n";
        for (int n = 1; n <= max_n; ++n)
            std::cout << n << ',' << tangencies[n].N << ',' << tangencies[n].O << '\n';
        return 0;
    }

    const auto scratch = argc > 2 ? argv[2] : "scratch";
    const unsigned partitions = argc > 3 ? std::stoul(argv[3]) : 0;
    const unsigned threads = argc > 4 ? std::stoul(argv[4])
        : std::max(1u, std::thread::hardware_concurrency());
    const auto g = meandric::count_indecomposable(max_n, scratch, partitions, threads);
    std::vector<meandric::Count> s(max_n + 1);
    s[0] = 1;
    std::cout << "n,g_n,s_n,N_n,O_n\n";
    for (int n = 1; n <= max_n; ++n) {
        for (int k = 1; k <= n; ++k)
            s[n] += g[k] * s[n - k];
        std::cout << n << ',' << g[n] << ',' << s[n] << ','
            << tangencies[n].N << ',' << tangencies[n].O << '\n';
    }
}
