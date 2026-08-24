#include "engine/RandomSource.h"

namespace pmg {

RandomSource::RandomSource(uint64_t seed) : m_engine(seed) {}

double RandomSource::NextUniform01() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(m_engine);
}

int RandomSource::NextWeightedIndex(const std::vector<float>& weights) {
    if (weights.empty()) {
        return -1;
    }

    double total = 0.0;
    for (float w : weights) {
        total += (w > 0.0f ? w : 0.0f);
    }
    if (total <= 0.0) {
        return 0;
    }

    double r = NextUniform01() * total;
    double cumulative = 0.0;
    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += (weights[i] > 0.0f ? weights[i] : 0.0f);
        if (r < cumulative) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(weights.size() - 1);
}

bool RandomSource::NextBernoulli(double probability) {
    if (probability <= 0.0) return false;
    if (probability >= 1.0) return true;
    return NextUniform01() < probability;
}

} // namespace pmg
