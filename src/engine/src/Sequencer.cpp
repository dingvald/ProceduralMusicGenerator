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
      m_currentPatternId(m_config.startPattern) {}

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

void Sequencer::EvaluateVariationForBar(int barIndex) {
    std::vector<VariationDecision> decisions = m_variationEngine.Evaluate(barIndex, m_currentPatternId, m_randomSource);

    for (const VariationDecision& decision : decisions) {
        switch (decision.type) {
            case VariationDecision::Type::NoOp:
                break;

            case VariationDecision::Type::SwapPattern:
                if (decision.targetId != m_currentPatternId && FindPattern(decision.targetId) != nullptr) {
                    if (m_logCallback) {
                        m_logCallback("[variation] bar " + std::to_string(barIndex) + ": swapping pattern '" +
                                      m_currentPatternId + "' -> '" + decision.targetId + "'");
                    }
                    m_currentPatternId = decision.targetId;
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
        }
    }
}

void Sequencer::Update() {
    uint64_t frames = m_audioEngine.GetFramesProcessed();
    double currentGlobalBeat = FramesToBeats(frames);

    int beatsPerBar = m_config.tempo.beatsPerBar > 0 ? m_config.tempo.beatsPerBar : 4;
    int targetBar = static_cast<int>(currentGlobalBeat / beatsPerBar);

    std::string patternBeforeVariation = m_currentPatternId;
    while (m_currentBar < targetBar) {
        int nextBar = m_currentBar + 1;
        EvaluateVariationForBar(nextBar);
        m_currentBar = nextBar;
    }

    const Pattern* pattern = FindPattern(m_currentPatternId);
    if (!pattern) {
        return;
    }

    if (m_currentPatternId != patternBeforeVariation) {
        // Swapped mid-update: restart the newly active pattern cleanly from
        // its own beat 0, rather than wherever it would land if its cycle
        // were phase-locked to the old pattern's.
        m_patternStartBeat = currentGlobalBeat;
        m_nextStepIndex = 0;
    }

    double patternLengthBeats = (pattern->lengthBars > 0 ? pattern->lengthBars : 1) * static_cast<double>(beatsPerBar);
    double patternBeat = currentGlobalBeat - m_patternStartBeat;
    if (patternBeat >= patternLengthBeats) {
        // Completed one or more full cycles of the pattern's own length;
        // advance the anchor by whole cycles (not to "now") so the pattern
        // stays phase-locked to the beat grid instead of drifting.
        double cyclesElapsed = std::floor(patternBeat / patternLengthBeats);
        m_patternStartBeat += cyclesElapsed * patternLengthBeats;
        patternBeat = currentGlobalBeat - m_patternStartBeat;
        m_nextStepIndex = 0;
    }

    while (m_nextStepIndex < pattern->steps.size() && pattern->steps[m_nextStepIndex].beatOffset <= patternBeat) {
        FireStep(pattern->steps[m_nextStepIndex]);
        ++m_nextStepIndex;
    }
}

} // namespace pmg
