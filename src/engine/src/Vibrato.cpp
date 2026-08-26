#include "engine/Vibrato.h"

#include <cmath>

namespace pmg {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void Vibrato::Configure(const VibratoConfig& config, uint32_t sampleRate) {
    m_config = config;

    float rate = m_config.rateHz > 0.0f ? m_config.rateHz : 1.0f;
    uint32_t sr = sampleRate > 0 ? sampleRate : 48000;
    m_phaseIncrement = static_cast<double>(rate) / static_cast<double>(sr);

    m_phase = 0.0;
}

void Vibrato::NoteOn() {
    m_phase = 0.0;
}

float Vibrato::NextMultiplier() {
    if (m_config.depthCents == 0.0f) {
        return 1.0f;
    }

    double lfo = std::sin(2.0 * kPi * m_phase);
    float semitones = static_cast<float>(m_config.depthCents) / 100.0f * static_cast<float>(lfo);

    m_phase += m_phaseIncrement;
    if (m_phase >= 1.0) {
        m_phase -= std::floor(m_phase);
    }

    return std::pow(2.0f, semitones / 12.0f);
}

} // namespace pmg
