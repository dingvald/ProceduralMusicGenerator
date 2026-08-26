#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/Arpeggiator.h"
#include "engine/DelayProcessor.h"
#include "engine/Envelope.h"
#include "engine/FmConfig.h"
#include "engine/LoFiProcessor.h"
#include "engine/Oscillator.h"
#include "engine/Theory.h"
#include "engine/Vibrato.h"

namespace pmg {

// Plain structs mirroring the JSON composition schema 1:1. Deliberately
// dumb data — nlohmann/json stays an edge-only dependency confined to
// ConfigLoader; nothing below it depends on JSON.

struct TempoConfig {
    double bpm = 120.0;
    int beatsPerBar = 4;
};

enum class InstrumentType { Synth, Sample };

struct InstrumentConfig {
    std::string id;
    InstrumentType type = InstrumentType::Synth;
    float gain = 1.0f;

    // Synth fields (type == Synth)
    Waveform waveform = Waveform::Sine;
    float dutyCycle = 0.5f; // only meaningful for waveform == Square
    ArpeggioConfig arpeggio; // empty semitoneOffsets (the default) = disabled
    ADSRParams envelope;
    VibratoConfig vibrato; // depthCents == 0 (the default) = disabled
    FmConfig fm;            // amount == 0 (the default) = disabled; only audible when waveform == Sine

    // Sample fields (type == Sample)
    std::string file;
};

struct StepConfig {
    double beat = 0.0;
    std::string instrument;
    bool hasNote = false;        // true for a pitched synth step; false = sample trigger
    NoteName note = NoteName::C; // meaningful only when hasNote
    int octave = 4;              // meaningful only when hasNote
    float velocity = 1.0f;
    float gate = 0.25f;
};

// A compact note-string melody block, expanded into StepConfig entries at
// load time (see NoteStringParser) and merged into a pattern's steps. Lets
// an author write "A B G F#" instead of a full step object per note.
struct MelodyConfig {
    std::string instrument;
    std::string notes;            // e.g. "A B G F#"
    double startBeat = 0.0;
    int defaultOctave = 4;
    double noteLengthBeats = 1.0; // default per-token slot duration, in beats
    float gateFraction = 0.8f;    // fraction of the slot actually held before auto-release
    float velocity = 0.8f;
};

// Config for a scale-constrained, procedurally-generated melody (see
// MelodyGenerator::GenerateMelody). Distinct from MelodyConfig above, which
// expands an author-written note-string instead of generating one.
struct GeneratedMelodyConfig {
    std::string instrument;
    NoteName key = NoteName::C;
    Scale scale = Scale::MajorPentatonic;
    int baseOctave = 4;
    int octaveRange = 1;          // random walk stays within [baseOctave, baseOctave + octaveRange]
    double lengthBeats = 8.0;
    double noteLengthBeats = 1.0; // per-slot duration, in beats
    float restProbability = 0.15f;
    float gateFraction = 0.8f;    // fraction of noteLengthBeats actually held before auto-release
    float velocity = 0.8f;
};

// Config for a procedurally-generated Euclidean rhythm (see
// RhythmGenerator::GenerateRhythm) -- distributes `pulses` hits as evenly as
// possible across `steps` slots via Bjorklund's algorithm.
struct GeneratedRhythmConfig {
    std::string instrument;
    int steps = 16; // grid resolution
    int pulses = 5; // how many of `steps` are hits
    double lengthBeats = 4.0;
    bool hasNote = false; // mirrors StepConfig::hasNote -- set for a pitched/noise-clocked hit
    NoteName note = NoteName::C;
    int octave = 8;
    float velocity = 0.6f;
    float velocityJitter = 0.0f; // +/- random velocity offset per hit (0 = none, deterministic)
    float gate = 0.1f;
};

struct PatternConfig {
    std::string id;
    int lengthBars = 1;
    std::vector<StepConfig> steps;
    std::vector<MelodyConfig> melodies;
    std::vector<GeneratedMelodyConfig> generatedMelodies;
    std::vector<GeneratedRhythmConfig> generatedRhythms;
};

enum class VariationOptionType { NoOp, SwapPattern, SetTrackMuted, AddLayer, RemoveLayer };

struct VariationOptionConfig {
    VariationOptionType type = VariationOptionType::NoOp;
    std::string targetId;   // pattern id (SwapPattern/AddLayer/RemoveLayer) or track id (SetTrackMuted)
    bool boolValue = false; // muted state (SetTrackMuted)
    float weight = 1.0f;
};

struct VariationRuleConfig {
    std::string id;
    std::string scope = "perBar";
    std::vector<VariationOptionConfig> options;
};

// One weighted outgoing edge in a Markov chain over pattern identity.
struct MarkovTransitionConfig {
    std::string toPattern;
    float weight = 1.0f;
};

// Keyed by source pattern id -> its weighted outgoing transitions.
using MarkovChainConfig = std::unordered_map<std::string, std::vector<MarkovTransitionConfig>>;

// Selects which IVariationStrategy implementation VariationEngine should be
// constructed with. RuleBased is the default (backward-compatible with
// compositions that only define "variationRules").
enum class VariationStrategyKind { RuleBased, MarkovChain };

struct CompositionConfig {
    uint32_t sampleRate = 48000;
    TempoConfig tempo;
    std::string startPattern;
    std::vector<InstrumentConfig> instruments;
    std::vector<PatternConfig> patterns;
    LoFiConfig loFi;   // post-mix bit-depth/sample-hold quantization; defaults to a no-op
    DelayConfig delay; // post-mix echo, applied before loFi; mix == 0 (the default) = disabled
    VariationStrategyKind variationStrategy = VariationStrategyKind::RuleBased;
    std::vector<VariationRuleConfig> variationRules; // used when variationStrategy == RuleBased
    MarkovChainConfig markovChain;                   // used when variationStrategy == MarkovChain
};

} // namespace pmg
