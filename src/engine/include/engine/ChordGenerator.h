#pragma once

#include <vector>

#include "engine/CompositionConfig.h"

namespace pmg {

// GeneratedChordConfig is defined in CompositionConfig.h (alongside
// GeneratedMelodyConfig/GeneratedRhythmConfig) so PatternConfig can hold a
// vector of them without a circular include between this header and
// CompositionConfig.h.

// Generates a deterministic chord progression: each entry of
// config.degrees becomes one stacked chord (scale degrees d, d+2, d+4 --
// root/third/fifth -- plus d+6 when config.seventh) resolved via
// Theory::DegreeToNote and held for chordLengthBeats. All tones of one
// chord are emitted as separate StepConfigs sharing the same beat and
// instrument; Mixer::NoteOn already scans a free-voice pool per call, so
// simultaneous same-beat/same-instrument steps play back as a chord with
// no further engine changes.
//
// Pure function, no JSON dependency, no RandomSource (fully deterministic
// given `degrees`, same shape as RhythmGenerator's Euclidean placement).
// Throws std::runtime_error if degrees is empty or chordLengthBeats isn't
// positive.
std::vector<StepConfig> GenerateChords(const GeneratedChordConfig& config);

} // namespace pmg
