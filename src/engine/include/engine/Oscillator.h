#pragma once

#include <cstdint>

namespace pmg {

enum class Waveform { Sine, Saw, Square, Triangle, Noise };

// Phase-accumulator oscillator. Saw/square/triangle edges are band-limited
// with PolyBLEP correction to suppress aliasing near discontinuities; sine
// needs no correction since it has none. Noise is a 15-bit LFSR (matching
// the NES APU's noise channel) clocked once per period at the configured
// frequency, held between clocks — deliberately un-smoothed, like a real
// chip's percussion channel.
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

    // Resets phase (and the triangle integrator) so a retriggered note
    // starts its waveform cycle cleanly. Deliberately does NOT reseed the
    // noise LFSR: a real chip's noise generator free-runs off its own
    // clock independent of note triggers, so repeated hits of the same
    // noise instrument (e.g. a hi-hat) don't replay an identical
    // pseudo-random sequence every time.
    void Reset();

    // phaseModulation perturbs only the *value read* for this sample (the
    // running phase accumulator is untouched), matching phase-modulation FM
    // as real 2-op FM chips (Yamaha OPN/OPL) implemented it. Honored only
    // for Sine -- Saw/Square/Triangle ignore it, since perturbing their
    // PolyBLEP-corrected phase math would break the band-limiting
    // correction and doesn't correspond to any real FM chip behavior; Noise
    // ignores it too, since it isn't phase-driven.
    float NextSample(double phaseModulation = 0.0);

private:
    static double PolyBlep(double t, double phaseIncrement);
    double BandlimitedPulse(double phaseIncrement, double duty) const;
    void ShiftLfsr();

    uint32_t m_sampleRate = 48000;
    float m_frequency = 440.0f;
    Waveform m_waveform = Waveform::Sine;
    double m_dutyCycle = 0.5;
    double m_phase = 0.0; // normalized [0, 1)
    double m_triangleIntegratorState = 0.0;
    uint16_t m_lfsrState = 1u;   // must stay nonzero, or the LFSR gets stuck at 0
    double m_noiseOutput = 1.0;  // held between LFSR shifts
};

} // namespace pmg
