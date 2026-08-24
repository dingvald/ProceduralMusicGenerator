#include <vector>

#include "doctest/doctest.h"
#include "engine/RandomSource.h"

using namespace pmg;

TEST_CASE("RandomSource::NextWeightedIndex distribution sanity") {
    RandomSource rng(7);
    std::vector<float> weights = {1.0f, 0.0f, 3.0f};

    const int trials = 5000;
    std::vector<int> counts(weights.size(), 0);
    for (int i = 0; i < trials; ++i) {
        int idx = rng.NextWeightedIndex(weights);
        REQUIRE(idx >= 0);
        REQUIRE(idx < static_cast<int>(weights.size()));
        counts[static_cast<size_t>(idx)]++;
    }

    CHECK(counts[1] == 0); // zero-weight option never chosen

    double freq0 = static_cast<double>(counts[0]) / trials;
    double freq2 = static_cast<double>(counts[2]) / trials;
    CHECK(freq0 == doctest::Approx(0.25).epsilon(0.1));
    CHECK(freq2 == doctest::Approx(0.75).epsilon(0.1));
}

TEST_CASE("RandomSource::NextWeightedIndex handles empty input") {
    RandomSource rng(1);
    std::vector<float> empty;
    CHECK(rng.NextWeightedIndex(empty) == -1);
}

TEST_CASE("RandomSource::NextBernoulli boundary behavior") {
    RandomSource rng(2);
    CHECK_FALSE(rng.NextBernoulli(0.0));
    CHECK(rng.NextBernoulli(1.0));
}
