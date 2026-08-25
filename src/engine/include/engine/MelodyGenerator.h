#pragma once

#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/RandomSource.h"

namespace pmg {

// GeneratedMelodyConfig is defined in CompositionConfig.h (alongside
// MelodyConfig) so PatternConfig can hold a vector of them without a
// circular include between this header and CompositionConfig.h.

// Generates a scale-constrained random-walk melody: starting on the root
// (scale degree 0), each slot either rests (rng.NextBernoulli(restProbability))
// or moves by a weighted random scale-degree delta biased toward small steps
// (+-1/+-2 dominate over +-3), clamped to stay within
// [baseOctave, baseOctave + octaveRange]. Every emitted note is guaranteed to
// lie in `scale` starting from `key` -- this is what constrains the output to
// sound musical rather than picking arbitrary pitches.
//
// Pure function, no JSON dependency (same shape as NoteStringParser::ParseNoteString):
// callers own the RandomSource, so a fixed seed yields a fully reproducible melody.
// Throws std::runtime_error if lengthBeats/noteLengthBeats aren't positive or
// octaveRange is negative.
std::vector<StepConfig> GenerateMelody(const GeneratedMelodyConfig& config, RandomSource& rng);

} // namespace pmg
