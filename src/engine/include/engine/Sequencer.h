#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/CompositionConfig.h"
#include "engine/Pattern.h"
#include "engine/RandomSource.h"

namespace pmg {

class AudioEngine;
class VariationEngine;
struct ResolvedStep;

// Control-thread clock driver. Converts AudioEngine::GetFramesProcessed()
// into elapsed beats (avoids wall-clock drift vs. a wall-clock timer), walks
// the current Pattern's steps, and pushes NoteOn/TriggerSample commands
// onto the AudioEngine's ParameterBus. At every bar boundary (tempo's fixed
// beatsPerBar) it asks VariationEngine for decisions and applies them
// (pattern swap / track mute), logging each one via the optional callback.
// The step-scan cursor loops on the CURRENT pattern's own length
// (lengthBars * beatsPerBar), independent of that fixed bar cadence, so a
// pattern longer than one bar plays out in full before repeating.
class Sequencer {
public:
    using VariationLogCallback = std::function<void(const std::string&)>;

    Sequencer(AudioEngine& audioEngine,
              VariationEngine& variationEngine,
              RandomSource& randomSource,
              CompositionConfig config,
              std::vector<Pattern> patterns);

    // Call frequently (e.g. every few milliseconds) from the control loop.
    void Update();

    void SetVariationLogCallback(VariationLogCallback callback);

private:
    const Pattern* FindPattern(const std::string& id) const;
    double FramesToBeats(uint64_t frames) const;
    void FireStep(const ResolvedStep& step);
    void EvaluateVariationForBar(int barIndex);

    AudioEngine& m_audioEngine;
    VariationEngine& m_variationEngine;
    RandomSource& m_randomSource;
    CompositionConfig m_config;
    std::vector<Pattern> m_patterns;

    std::string m_currentPatternId;
    int m_currentBar = -1;
    size_t m_nextStepIndex = 0;
    double m_patternStartBeat = 0.0; // global beat at which m_currentPatternId's current cycle began
    VariationLogCallback m_logCallback;
};

} // namespace pmg
