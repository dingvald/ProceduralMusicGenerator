#pragma once

#include <cstdint>
#include <limits>
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
    // -1.0 hard left .. 0.0 center (default) .. +1.0 hard right. Linear pan
    // law, not equal-power -- period hardware (Game Boy NR51 hard L/R/both
    // routing, SNES per-voice L/R volume registers) never did smooth
    // psychoacoustic panning either, and pan == 0.0 must reproduce the
    // pre-panning mono-summed loudness exactly (gain 1.0 on both channels).
    float pan = 0.0f;

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

// Config for a deterministic, explicit-progression chord generator (see
// ChordGenerator::GenerateChords). Fully deterministic given `degrees` (no
// RandomSource), like RhythmGenerator's hit placement. Each entry of
// `degrees` becomes one stacked chord (root/third/fifth, scale degrees
// d/d+2/d+4, plus d+6 when seventh == true) held for chordLengthBeats, all
// tones emitted as separate StepConfigs at the same beat on `instrument` --
// Mixer::NoteOn already scans a free-voice pool per call, so simultaneous
// same-beat/same-instrument steps play as a chord with no engine changes.
// Best suited to a 7-tone scale (Major/NaturalMinor/HarmonicMinor/Dorian/
// Mixolydian); pentatonic/Blues scales are accepted but the d+2/d+4/d+6
// stacking won't land on conventional triad intervals.
struct GeneratedChordConfig {
    std::string instrument;
    NoteName key = NoteName::C;
    Scale scale = Scale::Major;
    int baseOctave = 3;
    std::vector<int> degrees;        // e.g. {0,3,4,0} == I-IV-V-I; required, non-empty
    double chordLengthBeats = 4.0;   // beats each progression entry occupies
    bool seventh = false;            // false = triad; true = adds the d+6 seventh
    float velocity = 0.7f;
    float gateFraction = 0.9f;       // fraction of chordLengthBeats held before release
};

// Config for a procedurally-walked bass line locked to the SAME explicit
// scale-degree progression as a GeneratedChordConfig (see
// BasslineGenerator::GenerateBassline). Deliberately duplicates
// degrees/key/scale/baseOctave/chordLengthBeats rather than referencing
// GeneratedChordConfig, matching this codebase's independent, uncoupled
// generator-block philosophy; keep the two in sync manually when pairing
// them under one progression.
struct GeneratedBasslineConfig {
    std::string instrument;
    NoteName key = NoteName::C;
    Scale scale = Scale::Major;
    int baseOctave = 2;
    std::vector<int> degrees;             // same progression as the paired chord block
    double chordLengthBeats = 4.0;        // must match the chord progression's span
    double noteLengthBeats = 1.0;         // subdivision within each chord span
    float passingToneProbability = 0.2f;  // chance a non-first subdivision leaves the root
    float velocity = 0.75f;
    float gateFraction = 0.8f;
};

struct PatternConfig {
    std::string id;
    int lengthBars = 1;
    std::vector<StepConfig> steps;
    std::vector<MelodyConfig> melodies;
    std::vector<GeneratedMelodyConfig> generatedMelodies;
    std::vector<GeneratedRhythmConfig> generatedRhythms;
    std::vector<GeneratedChordConfig> generatedChords;
    std::vector<GeneratedBasslineConfig> generatedBasslines;
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

    // Optional runtime-parameter gate (see GameParameters): when
    // gateParameter is non-empty, this rule is only in scope for a given
    // bar's VariationEngine::Evaluate() while that parameter's current
    // value lies within [gateMin, gateMax] -- e.g. a "danger" parameter
    // gating a rule that only swaps to a more intense pattern once danger
    // crosses some threshold. An empty gateParameter (the default) means
    // "no gate" -- the rule is always in scope, exactly like before this
    // field existed. Only meaningful for RuleBasedVariationStrategy;
    // MarkovChainVariationStrategy never reads rulesInScope.
    std::string gateParameter;
    float gateMin = -std::numeric_limits<float>::infinity();
    float gateMax = std::numeric_limits<float>::infinity();
};

// A runtime parameter a host application (e.g. a game) can set live via
// GameParameters::SetParameter, read by VariationRuleConfig's gate and by
// GainCrossfadeConfig. Declaring the full set up front (rather than
// discovering names ad hoc from SetParameter calls) is what lets
// GameParameters stay a fixed set of atomics with no locking needed after
// construction -- see GameParameters.h.
struct GameParameterConfig {
    std::string name;
    float defaultValue = 0.0f;
};

// Maps a runtime parameter's value onto one instrument's Mixer track gain,
// smoothed over time (see GainCrossfader) rather than snapped instantly --
// e.g. a tension pad fading in as a "danger" parameter rises from 0 to 1.
// paramAtGainMin/paramAtGainMax need not be the parameter's own declared
// range; the mapping between them is linear and clamped outside it. Does
// not reference GameParameterConfig, matching this codebase's existing
// preference for independent, uncoupled config blocks (see
// GeneratedBasslineConfig's header comment) -- the composition author picks
// matching parameter names by convention, not by cross-reference.
struct GainCrossfadeConfig {
    std::string instrument;
    std::string parameter;
    float paramAtGainMin = 0.0f;
    float gainAtMin = 0.0f;
    float paramAtGainMax = 1.0f;
    float gainAtMax = 1.0f;
    float smoothingSeconds = 0.5f; // time to cross the full gainAtMin..gainAtMax range; <= 0 snaps instantly
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
    std::vector<GameParameterConfig> gameParameters;  // declared runtime parameters; see GameParameters
    std::vector<GainCrossfadeConfig> gainCrossfades;  // parameter-driven per-instrument gain fades
};

} // namespace pmg
