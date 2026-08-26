#pragma once

#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/RandomSource.h"

namespace pmg {

// GeneratedBasslineConfig is defined in CompositionConfig.h (alongside
// GeneratedChordConfig) so PatternConfig can hold a vector of them without
// a circular include between this header and CompositionConfig.h.

// Generates a bass line locked to the same explicit scale-degree
// progression a GeneratedChordConfig would use (config.degrees). Each
// progression entry spans chordLengthBeats, subdivided into
// noteLengthBeats-sized slots; the first slot of every span always plays
// the chord root (degree + 0) so the bass line stays harmonically
// anchored, while later slots in the span roll
// rng.NextBernoulli(passingToneProbability) to occasionally leave the root
// for the third or fifth (weighted like MelodyGenerator's delta table,
// biased toward staying put).
//
// Pure function, no JSON dependency (same shape as MelodyGenerator):
// callers own the RandomSource, so a fixed seed yields fully reproducible
// output. Throws std::runtime_error if degrees is empty or
// chordLengthBeats/noteLengthBeats aren't positive.
std::vector<StepConfig> GenerateBassline(const GeneratedBasslineConfig& config, RandomSource& rng);

} // namespace pmg
