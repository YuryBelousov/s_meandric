#include "numbers.hpp"
#include "tangency_profiles.hpp"

namespace meandric {

std::vector<TangencyNumbers> count_tangencies(int max_n) {
    using namespace tangency;
    std::vector<FactorSet> excluded(max_n + 1);
    std::vector<TangencyNumbers> numbers(max_n + 1);
    for (int n = 1; n <= max_n; ++n) {
        FactorSet polynomials;
        polynomials.reserve(8 * numbers[n - 1].N / 5 + 8);
        excluded[n].reserve(3 * excluded[n - 1].size() / 2 + 8);
        all_profiles(n, [&](const Factors& profile) {
            bool reachable = n == 1 ? profile.bridges == 1
                : has_predecessor(profile, n, excluded);
            if (!reachable) {
                excluded[n].insert(profile);
                return;
            }
            auto key = signature(profile);
            if (polynomials.insert(key) && constant_one(key)) ++numbers[n].O;
        });
        numbers[n].N = polynomials.size();
    }
    return numbers;
}

} // namespace meandric
