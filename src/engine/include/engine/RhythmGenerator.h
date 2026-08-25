#pragma once

#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/RandomSource.h"

namespace pmg {

// GeneratedRhythmConfig is defined in CompositionConfig.h (alongside
// MelodyConfig) so PatternConfig can hold a vector of them without a
// circular include between this header and CompositionConfig.h.

// Generates a Euclidean rhythm: pulses spread as evenly as possible over
// `steps` slots across `lengthBeats`, via the classic two-list Bjorklund
// construction (matches the canonical patterns from Toussaint's "The
// Euclidean Algorithm Generates Traditional Musical Rhythms", e.g.
// GenerateRhythm({steps=8, pulses=3, ...}) places hits at slots 0,3,6 --
// "10010010"). `pulses <= 0` yields no hits; `pulses >= steps` yields every
// slot. `rng` drives only the optional per-hit velocityJitter -- with
// velocityJitter == 0 (the default) two calls with the same config produce
// bit-identical output regardless of rng state.
//
// Pure function, no JSON dependency (same shape as
// NoteStringParser::ParseNoteString / GenerateMelody).
std::vector<StepConfig> GenerateRhythm(const GeneratedRhythmConfig& config, RandomSource& rng);

} // namespace pmg
