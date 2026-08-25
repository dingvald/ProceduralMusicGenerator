#include "engine/Sequencer.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "engine/AudioEngine.h"
#include "engine/ParameterBus.h"
#include "engine/VariationEngine.h"

namespace pmg {

Sequencer::Sequencer(AudioEngine& audioEngine,
                      VariationEngine& variationEngine,
                      RandomSource& randomSource,
                      CompositionConfig config,
                      std::vector<Pattern> patterns)
    : m_audioEngine(audioEngine),
      m_variationEngine(variationEngine),
      m_randomSource(randomSource),
      m_config(std::move(config)),
      m_patterns(std::move(patterns)),
      m_activeLayers{PatternLayer{m_config.startPattern, 0, 0.0}} {}

void Sequencer::SetVariationLogCallback(VariationLogCallback callback) {
    m_logCallback = std::move(callback);
}

const Pattern* Sequencer::FindPattern(const std::string& id) const {
    for (const Pattern& pattern : m_patterns) {
        if (pattern.id == id) {
            return &pattern;
        }
    }
    return nullptr;
}

double Sequencer::FramesToBeats(uint64_t frames) const {
    double seconds = static_cast<double>(frames) / static_cast<double>(m_audioEngine.GetSampleRate());
    double beatsPerSecond = m_config.tempo.bpm / 60.0;
    return seconds * beatsPerSecond;
}

void Sequencer::FireStep(const ResolvedStep& step) {
    Command command;
    command.id = FixedId(step.instrument);
    if (step.isSynth) {
        command.type = CommandType::NoteOn;
        command.floatValue = step.frequencyHz;
        command.floatValue2 = step.velocity;

        // step.gate is a fraction of a beat; convert to a sample count so
        // the voice can auto-release itself with no further scheduling from
        // this (control) thread -- the audio thread never learns which
        // voice handle a NoteOn was assigned, so a separately-scheduled
        // NoteOff command could not target the right voice anyway.
        double secondsPerBeat = 60.0 / m_config.tempo.bpm;
        double gateDurationSamples = step.gate * secondsPerBeat * m_audioEngine.GetSampleRate();
        command.intValue = std::max(1, static_cast<int>(gateDurationSamples));
    } else {
        command.type = CommandType::TriggerSample;
        command.floatValue = step.velocity;
    }
    m_audioEngine.GetParameterBus().Push(command);
}

void Sequencer::EvaluateVariationForBar(int barIndex, double currentGlobalBeat) {
    // The base layer (index 0) is what VariationEngine reasons about as
    // "the current pattern" -- Markov-chain transitions and swapPattern
    // rules are only ever defined relative to it; extra layers are purely
    // additive on top.
    const std::string& basePatternId = m_activeLayers[0].patternId;
    std::vector<VariationDecision> decisions = m_variationEngine.Evaluate(barIndex, basePatternId, m_randomSource);

    for (const VariationDecision& decision : decisions) {
        switch (decision.type) {
            case VariationDecision::Type::NoOp:
                break;

            case VariationDecision::Type::SwapPattern:
                if (decision.targetId != m_activeLayers[0].patternId && FindPattern(decision.targetId) != nullptr) {
                    if (m_logCallback) {
                        m_logCallback("[variation] bar " + std::to_string(barIndex) + ": swapping pattern '" +
                                      m_activeLayers[0].patternId + "' -> '" + decision.targetId + "'");
                    }
                    m_activeLayers[0].patternId = decision.targetId;
                    m_activeLayers[0].patternStartBeat = currentGlobalBeat;
                    m_activeLayers[0].nextStepIndex = 0;
                }
                break;

            case VariationDecision::Type::SetTrackMuted: {
                Command command;
                command.type = CommandType::SetTrackMuted;
                command.id = FixedId(decision.targetId);
                command.boolValue = decision.boolValue;
                m_audioEngine.GetParameterBus().Push(command);
                if (m_logCallback) {
                    m_logCallback("[variation] bar " + std::to_string(barIndex) + ": track '" + decision.targetId +
                                  "' " + (decision.boolValue ? "muted" : "unmuted"));
                }
                break;
            }

            case VariationDecision::Type::AddLayer: {
                bool alreadyActive = false;
                for (const PatternLayer& layer : m_activeLayers) {
                    if (layer.patternId == decision.targetId) {
                        alreadyActive = true;
                        break;
                    }
                }
                if (!alreadyActive && FindPattern(decision.targetId) != nullptr) {
                    // Starts fresh from the new pattern's own beat 0, right
                    // now, independent of every other active layer's phase.
                    m_activeLayers.push_back(PatternLayer{decision.targetId, 0, currentGlobalBeat});
                    if (m_logCallback) {
                        m_logCallback("[variation] bar " + std::to_string(barIndex) + ": adding layer '" +
                                      decision.targetId + "'");
                    }
                }
                break;
            }

            case VariationDecision::Type::RemoveLayer: {
                // Layer 0 (the base) is never removable this way -- only
                // layers added via AddLayer, so a composition can never end
                // up with zero active layers (permanent silence).
                for (size_t i = 1; i < m_activeLayers.size(); ++i) {
                    if (m_activeLayers[i].patternId == decision.targetId) {
                        m_activeLayers.erase(m_activeLayers.begin() + static_cast<std::ptrdiff_t>(i));
                        if (m_logCallback) {
                            m_logCallback("[variation] bar " + std::to_string(barIndex) + ": removing layer '" +
                                          decision.targetId + "'");
                        }
                        break;
                    }
                }
                break;
            }
        }
    }
}

void Sequencer::UpdateLayer(PatternLayer& layer, double currentGlobalBeat, int beatsPerBar) {
    const Pattern* pattern = FindPattern(layer.patternId);
    if (!pattern) {
        return;
    }

    double patternLengthBeats = (pattern->lengthBars > 0 ? pattern->lengthBars : 1) * static_cast<double>(beatsPerBar);
    double patternBeat = currentGlobalBeat - layer.patternStartBeat;
    if (patternBeat >= patternLengthBeats) {
        // Completed one or more full cycles of this layer's pattern length;
        // advance the anchor by whole cycles (not to "now") so playback
        // stays phase-locked to the beat grid instead of drifting.
        double cyclesElapsed = std::floor(patternBeat / patternLengthBeats);
        layer.patternStartBeat += cyclesElapsed * patternLengthBeats;
        patternBeat = currentGlobalBeat - layer.patternStartBeat;
        layer.nextStepIndex = 0;
    }

    while (layer.nextStepIndex < pattern->steps.size() && pattern->steps[layer.nextStepIndex].beatOffset <= patternBeat) {
        FireStep(pattern->steps[layer.nextStepIndex]);
        ++layer.nextStepIndex;
    }
}

void Sequencer::Update() {
    uint64_t frames = m_audioEngine.GetFramesProcessed();
    double currentGlobalBeat = FramesToBeats(frames);

    int beatsPerBar = m_config.tempo.beatsPerBar > 0 ? m_config.tempo.beatsPerBar : 4;
    int targetBar = static_cast<int>(currentGlobalBeat / beatsPerBar);

    while (m_currentBar < targetBar) {
        int nextBar = m_currentBar + 1;
        EvaluateVariationForBar(nextBar, currentGlobalBeat);
        m_currentBar = nextBar;
    }

    // Each active layer scans its own pattern independently -- a fresh
    // layer added moments ago and the long-running base layer can be at
    // completely different points in completely different cycle lengths.
    for (PatternLayer& layer : m_activeLayers) {
        UpdateLayer(layer, currentGlobalBeat, beatsPerBar);
    }
}

} // namespace pmg
