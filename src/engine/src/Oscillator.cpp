#include "engine/Oscillator.h"

#include <algorithm>
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
    m_triangleIntegratorState = 0.0;
}

// Residual correction subtracted/added at a discontinuity to replace the
// naive step with a band-limited transition covering +/-1 sample around it.
// t is the phase distance (normalized) from the discontinuity, wrapped into
// [0, 1); phaseIncrement is the current per-sample phase step (dt).
double Oscillator::PolyBlep(double t, double phaseIncrement) {
    if (phaseIncrement <= 0.0) {
        return 0.0;
    }
    if (t < phaseIncrement) {
        double x = t / phaseIncrement;
        return x + x - x * x - 1.0;
    }
    if (t > 1.0 - phaseIncrement) {
        double x = (t - 1.0) / phaseIncrement;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

double Oscillator::BandlimitedSquare(double phaseIncrement) const {
    double value = m_phase < 0.5 ? 1.0 : -1.0;
    value += PolyBlep(m_phase, phaseIncrement);
    value -= PolyBlep(std::fmod(m_phase + 0.5, 1.0), phaseIncrement);
    return value;
}

float Oscillator::NextSample() {
    double phaseIncrement = static_cast<double>(m_frequency) / static_cast<double>(m_sampleRate);

    float value = 0.0f;
    switch (m_waveform) {
        case Waveform::Sine:
            value = static_cast<float>(std::sin(2.0 * kPi * m_phase));
            break;
        case Waveform::Saw: {
            double naive = 2.0 * m_phase - 1.0;
            naive -= PolyBlep(m_phase, phaseIncrement);
            value = static_cast<float>(naive);
            break;
        }
        case Waveform::Square:
            value = static_cast<float>(BandlimitedSquare(phaseIncrement));
            break;
        case Waveform::Triangle: {
            // Leaky-integrate the band-limited square into a triangle, which
            // keeps the PolyBLEP correction and avoids DC drift; rescale back
            // to unit amplitude afterward. The *4 rescale assumes the filter
            // has settled into its periodic orbit; right after Reset() (or a
            // frequency jump) the integrator starts away from that orbit and
            // can transiently overshoot before it settles, so clamp.
            double square = BandlimitedSquare(phaseIncrement);
            m_triangleIntegratorState =
                phaseIncrement * square + (1.0 - phaseIncrement) * m_triangleIntegratorState;
            double triangle = 4.0 * m_triangleIntegratorState;
            triangle = std::max(-1.0, std::min(1.0, triangle));
            value = static_cast<float>(triangle);
            break;
        }
    }

    m_phase += phaseIncrement;
    if (m_phase >= 1.0) {
        m_phase -= std::floor(m_phase);
    } else if (m_phase < 0.0) {
        m_phase -= std::floor(m_phase);
    }

    return value;
}

} // namespace pmg
