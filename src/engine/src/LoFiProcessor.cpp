#include "engine/LoFiProcessor.h"

#include <algorithm>
#include <cmath>

namespace pmg {

void LoFiProcessor::Configure(const LoFiConfig& config) {
    m_config = config;
    m_config.bitDepth = std::max(1, std::min(16, m_config.bitDepth));
    m_config.holdFactor = std::max(1, m_config.holdFactor);
    m_heldSample = 0.0f;
    m_holdCounter = 0;
}

float LoFiProcessor::Process(float input) {
    if (m_holdCounter == 0) {
        m_heldSample = input;
    }
    m_holdCounter = (m_holdCounter + 1) % m_config.holdFactor;

    if (m_config.bitDepth >= 16) {
        return m_heldSample; // effectively unquantized
    }

    float clamped = std::max(-1.0f, std::min(1.0f, m_heldSample));
    float levels = static_cast<float>((1 << m_config.bitDepth) - 1);
    float normalized = (clamped * 0.5f + 0.5f) * levels; // map [-1, 1] -> [0, levels]
    float quantized = std::round(normalized) / levels;
    return quantized * 2.0f - 1.0f; // map back to [-1, 1]
}

} // namespace pmg
