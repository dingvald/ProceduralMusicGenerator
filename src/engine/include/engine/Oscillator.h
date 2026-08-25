#pragma once

#include <cstdint>

namespace pmg {

enum class Waveform { Sine, Saw, Square, Triangle };

// Phase-accumulator oscillator. Saw/square/triangle edges are band-limited
// with PolyBLEP correction to suppress aliasing near discontinuities; sine
// needs no correction since it has none.
// Audio-thread safe: no allocation, no locking.
class Oscillator {
public:
    void SetSampleRate(uint32_t sampleRate);
    void SetFrequency(float hz);
    void SetWaveform(Waveform waveform);

    // Fraction of the period spent at +1 before falling to -1, in (0, 1).
    // Only Square uses this (real chip "pulse" channels vary this for
    // timbre, e.g. NES's 12.5/25/50/75% duty options); Triangle always
    // integrates a fixed 50% pulse regardless of this setting, since a
    // chip's triangle channel has no duty control. Clamped to [0.01, 0.99]
    // to keep both edges resolvable at audio rate.
    void SetDutyCycle(float duty);

    void Reset();

    float NextSample();

private:
    static double PolyBlep(double t, double phaseIncrement);
    double BandlimitedPulse(double phaseIncrement, double duty) const;

    uint32_t m_sampleRate = 48000;
    float m_frequency = 440.0f;
    Waveform m_waveform = Waveform::Sine;
    double m_dutyCycle = 0.5;
    double m_phase = 0.0; // normalized [0, 1)
    double m_triangleIntegratorState = 0.0;
};

} // namespace pmg
