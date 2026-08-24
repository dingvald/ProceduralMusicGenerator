#include "engine/Oscillator.h"

#include <cmath>

namespace pmg {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void Oscillator::SetSampleRate(uint32_t sampleRate) {
    m_sampleRate = sampleRate > 0 ? sampleRate : 48000;
}

void Oscillator::SetFrequency(float hz) {
    m_frequency = hz;
}

void Oscillator::SetWaveform(Waveform waveform) {
    m_waveform = waveform;
}

void Oscillator::Reset() {
    m_phase = 0.0;
}

float Oscillator::NextSample() {
    float value = 0.0f;
    switch (m_waveform) {
        case Waveform::Sine:
            value = static_cast<float>(std::sin(2.0 * kPi * m_phase));
            break;
        case Waveform::Saw:
            value = static_cast<float>(2.0 * m_phase - 1.0);
            break;
        case Waveform::Square:
            value = m_phase < 0.5 ? 1.0f : -1.0f;
            break;
        case Waveform::Triangle:
            value = static_cast<float>(4.0 * std::fabs(m_phase - 0.5) - 1.0);
            break;
    }

    double phaseIncrement = static_cast<double>(m_frequency) / static_cast<double>(m_sampleRate);
    m_phase += phaseIncrement;
    if (m_phase >= 1.0) {
        m_phase -= std::floor(m_phase);
    } else if (m_phase < 0.0) {
        m_phase -= std::floor(m_phase);
    }

    return value;
}

} // namespace pmg
