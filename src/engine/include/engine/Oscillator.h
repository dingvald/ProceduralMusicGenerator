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
    void Reset();

    float NextSample();

private:
    static double PolyBlep(double t, double phaseIncrement);
    double BandlimitedSquare(double phaseIncrement) const;

    uint32_t m_sampleRate = 48000;
    float m_frequency = 440.0f;
    Waveform m_waveform = Waveform::Sine;
    double m_phase = 0.0; // normalized [0, 1)
    double m_triangleIntegratorState = 0.0;
};

} // namespace pmg
