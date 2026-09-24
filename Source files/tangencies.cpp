#include "numbers.hpp"
#include "tangency_profiles.hpp"

namespace meandric {

std::vector<TangencyNumbers> count_tangencies(int max_n) {
    using namespace tangency;
    std::vector<Seed> seeds{{2, 2, 0, {}}, {2, 0, 1, {5}}};
    for (int n = 5; n <= max_n; ++n)
        for (int k = 3; k < n; ++k)
            if (allowed_monomial(n, k))
                seeds.push_back(monomial_seed(n, k));

    std::vector<ProfileSet> profiles(max_n + 1);
    profiles[1].insert(Profile{1});
    std::vector<TangencyNumbers> numbers(max_n + 1);
    for (int n = 1; n <= max_n; ++n) {
        PolynomialSet polynomials;
        for (const auto& profile : profiles[n])
            polynomials.insert(polynomial(profile));
        numbers[n].N = polynomials.size();
        for (const auto& coefficients : polynomials)
            if (coefficients[0] == 1)
                ++numbers[n].O;

        extend_profiles(profiles, n, seeds);
        ProfileSet().swap(profiles[n]);
    }
    return numbers;
}

} // namespace meandric
