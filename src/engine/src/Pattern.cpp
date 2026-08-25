#include "engine/Pattern.h"

#include <algorithm>

namespace pmg {

Pattern ResolvePattern(const PatternConfig& config) {
    Pattern pattern;
    pattern.id = config.id;
    pattern.lengthBars = config.lengthBars;
    pattern.steps.reserve(config.steps.size());

    for (const StepConfig& stepConfig : config.steps) {
        ResolvedStep step;
        step.beatOffset = stepConfig.beat;
        step.instrument = stepConfig.instrument;
        step.velocity = stepConfig.velocity;
        step.gate = stepConfig.gate;
        step.isSynth = stepConfig.hasNote;
        if (step.isSynth) {
            step.frequencyHz = Theory::NoteToFrequency(stepConfig.note, stepConfig.octave);
        }
        pattern.steps.push_back(step);
    }

    std::stable_sort(pattern.steps.begin(), pattern.steps.end(),
                      [](const ResolvedStep& a, const ResolvedStep& b) { return a.beatOffset < b.beatOffset; });

    return pattern;
}

} // namespace pmg
