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

void Oscillator::SetDutyCycle(float duty) {
    m_dutyCycle = std::max(0.01, std::min(0.99, static_cast<double>(duty)));
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

double Oscillator::BandlimitedPulse(double phaseIncrement, double duty) const {
    double value = m_phase < duty ? 1.0 : -1.0;
    value += PolyBlep(m_phase, phaseIncrement);

    double tFalling = m_phase - duty;
    if (tFalling < 0.0) {
        tFalling += 1.0;
    }
    value -= PolyBlep(tFalling, phaseIncrement);
    return value;
}

// 15-bit Fibonacci LFSR with taps at bit 0 and bit 1 (NES APU noise
// channel "mode 0" / long sequence). Shifting right by one bit each clock
// and feeding the XOR of the two low bits back into bit 14 produces a
// pseudo-random bitstream with a 32767-sample period before repeating.
void Oscillator::ShiftLfsr() {
    unsigned feedback = (m_lfsrState ^ (m_lfsrState >> 1)) & 1u;
    m_lfsrState = static_cast<uint16_t>((m_lfsrState >> 1) | (feedback << 14));
    m_noiseOutput = (m_lfsrState & 1u) ? 1.0 : -1.0;
}

float Oscillator::NextSample(double phaseModulation) {
    double phaseIncrement = static_cast<double>(m_frequency) / static_cast<double>(m_sampleRate);

    float value = 0.0f;
    switch (m_waveform) {
        case Waveform::Sine:
            value = static_cast<float>(std::sin(2.0 * kPi * (m_phase + phaseModulation)));
            break;
        case Waveform::Saw: {
            double naive = 2.0 * m_phase - 1.0;
            naive -= PolyBlep(m_phase, phaseIncrement);
            value = static_cast<float>(naive);
            break;
        }
        case Waveform::Square:
            value = static_cast<float>(BandlimitedPulse(phaseIncrement, m_dutyCycle));
            break;
        case Waveform::Triangle: {
            // Leaky-integrate a band-limited 50% pulse into a triangle, which
            // keeps the PolyBLEP correction and avoids DC drift; rescale back
            // to unit amplitude afterward. The *4 rescale assumes the filter
            // has settled into its periodic orbit; right after Reset() (or a
            // frequency jump) the integrator starts away from that orbit and
            // can transiently overshoot before it settles, so clamp. Always
            // 50% duty regardless of m_dutyCycle: a chip's triangle channel
            // has no duty control, so this stays fixed.
            double square = BandlimitedPulse(phaseIncrement, 0.5);
            m_triangleIntegratorState =
                phaseIncrement * square + (1.0 - phaseIncrement) * m_triangleIntegratorState;
            double triangle = 4.0 * m_triangleIntegratorState;
            triangle = std::max(-1.0, std::min(1.0, triangle));
            value = static_cast<float>(triangle);
            break;
        }
        case Waveform::Noise:
            value = static_cast<float>(m_noiseOutput); // held until the next LFSR clock, below
            break;
    }

    m_phase += phaseIncrement;
    if (m_phase >= 1.0) {
        m_phase -= std::floor(m_phase);
        if (m_waveform == Waveform::Noise) {
            ShiftLfsr();
        }
    } else if (m_phase < 0.0) {
        m_phase -= std::floor(m_phase);
        if (m_waveform == Waveform::Noise) {
            ShiftLfsr();
        }
    }

    return value;
}

} // namespace pmg
