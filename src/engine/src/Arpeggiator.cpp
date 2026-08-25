#include "engine/Arpeggiator.h"

#include <algorithm>
#include <cmath>

namespace pmg {

void Arpeggiator::Configure(const ArpeggioConfig& config, uint32_t sampleRate) {
    m_config = config;

    float rate = m_config.rateHz > 0.0f ? m_config.rateHz : 1.0f;
    uint32_t sr = sampleRate > 0 ? sampleRate : 48000;
    m_samplesPerStep = std::max(1u, static_cast<uint32_t>(static_cast<float>(sr) / rate));

    m_sampleCounter = 0;
    m_stepIndex = 0;
}

void Arpeggiator::NoteOn() {
    m_sampleCounter = 0;
    m_stepIndex = 0;
}

float Arpeggiator::NextMultiplier() {
    if (m_config.semitoneOffsets.empty()) {
        return 1.0f;
    }

    int semitones = m_config.semitoneOffsets[m_stepIndex % m_config.semitoneOffsets.size()];

    ++m_sampleCounter;
    if (m_sampleCounter >= m_samplesPerStep) {
        m_sampleCounter = 0;
        ++m_stepIndex;
    }

    return std::pow(2.0f, static_cast<float>(semitones) / 12.0f);
}

} // namespace pmg
