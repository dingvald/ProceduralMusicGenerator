#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/GainCrossfader.h"
#include "engine/GameParameters.h"
#include "engine/Pattern.h"
#include "engine/RandomSource.h"

namespace pmg {

class AudioEngine;
class VariationEngine;
struct ResolvedStep;

// One independently-timed pattern currently playing. Layer [0] is the
// "base" layer (seeded from the composition's startPattern; SwapPattern
// decisions only ever replace this one); layers at index >= 1 are added by
// AddLayer/RemoveLayer decisions and can play a pattern with completely
// different lengthBars/step timing than the base, overlapping it rather
// than replacing it.
struct PatternLayer {
    std::string patternId;
    size_t nextStepIndex = 0;
    double patternStartBeat = 0.0; // global beat at which this layer's current cycle began
};

// One resolved GainCrossfadeConfig entry: the instrument/parameter names
// pulled out where Sequencer can read them each tick, alongside the
// stateful GainCrossfader itself.
struct GainCrossfadeEntry {
    std::string instrument;
    std::string parameter;
    GainCrossfader crossfader;
};

// Control-thread clock driver. Converts AudioEngine::GetFramesProcessed()
// into elapsed beats (avoids wall-clock drift vs. a wall-clock timer), walks
// each active PatternLayer's steps, and pushes NoteOn/TriggerSample commands
// onto the AudioEngine's ParameterBus. At every bar boundary (tempo's fixed
// beatsPerBar) it asks VariationEngine for decisions and applies them
// (pattern swap / track mute / add or remove a layer), logging each one via
// the optional callback. Each layer's step-scan cursor loops on that
// layer's own pattern length (lengthBars * beatsPerBar), independent of the
// fixed bar cadence and independent of every other active layer, so a
// pattern longer than one bar plays out in full before repeating, and two
// layers of different lengths stay correctly out of phase with each other.
class Sequencer {
public:
    using VariationLogCallback = std::function<void(const std::string&)>;

    // gameParameters must outlive this Sequencer; read once per Update()
    // tick to drive any configured GainCrossfaders (config.gainCrossfades).
    Sequencer(AudioEngine& audioEngine,
              VariationEngine& variationEngine,
              RandomSource& randomSource,
              const GameParameters& gameParameters,
              CompositionConfig config,
              std::vector<Pattern> patterns);

    // Call frequently (e.g. every few milliseconds) from the control loop.
    void Update();

    void SetVariationLogCallback(VariationLogCallback callback);

private:
    const Pattern* FindPattern(const std::string& id) const;
    double FramesToBeats(uint64_t frames) const;
    void FireStep(const ResolvedStep& step);
    void EvaluateVariationForBar(int barIndex, double currentGlobalBeat);
    void UpdateLayer(PatternLayer& layer, double currentGlobalBeat, int beatsPerBar);
    void UpdateGainCrossfades(uint64_t frames);

    AudioEngine& m_audioEngine;
    VariationEngine& m_variationEngine;
    RandomSource& m_randomSource;
    const GameParameters& m_gameParameters;
    CompositionConfig m_config;
    std::vector<Pattern> m_patterns;

    std::vector<PatternLayer> m_activeLayers;
    int m_currentBar = -1;
    VariationLogCallback m_logCallback;

    // One entry per config.gainCrossfades entry, in the same order.
    std::vector<GainCrossfadeEntry> m_gainCrossfaders;
    uint64_t m_lastGainUpdateFrames = 0;
};

} // namespace pmg
