#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pmg {

// Cycles through a list of semitone offsets from a held note's base pitch
// at a fixed rate, faking a chord on a channel that can only play one
// pitch at a time -- real chip hardware commonly had only 1-3 melodic
// channels, so a fast single-channel arpeggio was the standard way to
// imply a chord. An empty offset list disables the arpeggio.
struct ArpeggioConfig {
    std::vector<int> semitoneOffsets; // e.g. {0, 4, 7} for a major triad; empty = disabled
    float rateHz = 20.0f;             // steps per second while a note is held
};

// Audio-thread safe: no allocation after Configure(), no locking.
class Arpeggiator {
public:
    void Configure(const ArpeggioConfig& config, uint32_t sampleRate);

    // Resets to the first offset in the pattern, so every newly triggered
    // note starts its arpeggio from the same place (matching how chip
    // trackers restart an arp on each new note) rather than wherever a
    // previous note's cycle happened to leave off.
    void NoteOn();

    // Frequency multiplier to apply for this sample: 2^(semitoneOffset/12).
    // Always 1.0 when disabled (no configured offsets), so multiplying an
    // unaffected voice's frequency by this is a no-op.
    float NextMultiplier();

private:
    ArpeggioConfig m_config;
    uint32_t m_samplesPerStep = 1;
    uint32_t m_sampleCounter = 0;
    size_t m_stepIndex = 0;
};

} // namespace pmg
