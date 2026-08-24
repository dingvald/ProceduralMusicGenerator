#pragma once

#include <cstdint>

namespace pmg {

enum class Waveform { Sine, Saw, Square, Triangle };

// Naive (non-band-limited) phase-accumulator oscillator. Aliasing at high
// frequencies is a known v1 limitation; PolyBLEP band-limiting is deferred.
// Audio-thread safe: no allocation, no locking.
class Oscillator {
public:
    void SetSampleRate(uint32_t sampleRate);
    void SetFrequency(float hz);
    void SetWaveform(Waveform waveform);
    void Reset();

    float NextSample();

private:
    uint32_t m_sampleRate = 48000;
    float m_frequency = 440.0f;
    Waveform m_waveform = Waveform::Sine;
    double m_phase = 0.0; // normalized [0, 1)
};

} // namespace pmg
