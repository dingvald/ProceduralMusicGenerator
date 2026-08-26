#pragma once

#include <cstdint>

namespace pmg {

// Sine-shaped pitch LFO, the standard chip-tracker/hardware auto-vibrato
// technique for adding pitch wobble to a held note. depthCents == 0 (the
// default) disables it entirely.
struct VibratoConfig {
    float rateHz = 5.0f;     // LFO cycles per second while a note is held
    float depthCents = 0.0f; // peak pitch deviation in cents; 0 = disabled
};

// Audio-thread safe: no allocation after Configure(), no locking.
class Vibrato {
public:
    void Configure(const VibratoConfig& config, uint32_t sampleRate);

    // Resets LFO phase to 0, so every newly triggered note starts its
    // vibrato cycle cleanly (matching Arpeggiator::NoteOn()'s precedent)
    // rather than continuing a previous note's phase.
    void NoteOn();

    // Frequency multiplier for this sample: 2^(depthCents/1200 * sin(phase)).
    // Always 1.0 when disabled (depthCents == 0).
    float NextMultiplier();

private:
    VibratoConfig m_config;
    double m_phase = 0.0; // normalized [0, 1)
    double m_phaseIncrement = 0.0;
};

} // namespace pmg
