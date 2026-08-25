#pragma once

#include <string>
#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/Theory.h"

namespace pmg {

// Resolved/runtime form of a pattern, distinct from the raw PatternConfig
// parsed from JSON: note-name steps have already been resolved to concrete
// frequencies.
struct ResolvedStep {
    double beatOffset = 0.0;
    std::string instrument;
    bool isSynth = false;     // true => frequencyHz applies (synth NoteOn); false => sample trigger
    float frequencyHz = 0.0f;
    float velocity = 1.0f;
    float gate = 0.25f;
};

struct Pattern {
    std::string id;
    int lengthBars = 1;
    std::vector<ResolvedStep> steps; // sorted ascending by beatOffset
};

// Resolves a PatternConfig's note-based steps into concrete frequencies via
// Theory::NoteToFrequency, once at load time (not per-note at runtime).
// Steps are sorted ascending by beat so Sequencer can walk them with a
// simple monotonically-advancing index.
Pattern ResolvePattern(const PatternConfig& config);

} // namespace pmg
