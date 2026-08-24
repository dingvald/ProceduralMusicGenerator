#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace pmg {

// Seedable RNG wrapper used only on the control thread (Sequencer,
// VariationEngine, config resolution). Never used from the audio callback.
class RandomSource {
public:
    explicit RandomSource(uint64_t seed);

    double NextUniform01();

    // Picks an index in [0, weights.size()) with probability proportional
    // to each entry's weight. Weights need not sum to 1; non-positive
    // weights are treated as zero. Returns -1 if weights is empty.
    int NextWeightedIndex(const std::vector<float>& weights);

    bool NextBernoulli(double probability);

private:
    std::mt19937_64 m_engine;
};

} // namespace pmg
