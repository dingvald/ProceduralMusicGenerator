#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/Envelope.h"
#include "engine/Oscillator.h"

namespace pmg {

// Plain structs mirroring the JSON composition schema 1:1. Deliberately
// dumb data — nlohmann/json stays an edge-only dependency confined to
// ConfigLoader; nothing below it depends on JSON.

struct TempoConfig {
    double bpm = 120.0;
    int beatsPerBar = 4;
};

struct KeyConfig {
    std::string root = "C";
    std::string scale = "major";
};

enum class InstrumentType { Synth, Sample };

struct InstrumentConfig {
    std::string id;
    InstrumentType type = InstrumentType::Synth;
    float gain = 1.0f;

    // Synth fields (type == Synth)
    Waveform waveform = Waveform::Sine;
    ADSRParams envelope;

    // Sample fields (type == Sample)
    std::string file;
};

struct StepConfig {
    double beat = 0.0;
    std::string instrument;
    bool hasDegree = false; // true for synth steps carrying a scale degree
    int degree = 0;
    float velocity = 1.0f;
    float gate = 0.25f;
};

struct PatternConfig {
    std::string id;
    int lengthBars = 1;
    std::vector<StepConfig> steps;
};

enum class VariationOptionType { NoOp, SwapPattern, SetTrackMuted };

struct VariationOptionConfig {
    VariationOptionType type = VariationOptionType::NoOp;
    std::string targetId;   // pattern id (SwapPattern) or track id (SetTrackMuted)
    bool boolValue = false; // muted state (SetTrackMuted)
    float weight = 1.0f;
};

struct VariationRuleConfig {
    std::string id;
    std::string scope = "perBar";
    std::vector<VariationOptionConfig> options;
};

struct CompositionConfig {
    uint32_t sampleRate = 48000;
    TempoConfig tempo;
    KeyConfig key;
    std::string startPattern;
    std::vector<InstrumentConfig> instruments;
    std::vector<PatternConfig> patterns;
    std::vector<VariationRuleConfig> variationRules;
};

} // namespace pmg
