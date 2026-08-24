#include "engine/Pattern.h"

#include <algorithm>

namespace pmg {

Pattern ResolvePattern(const PatternConfig& config, NoteName root, ScaleType scale) {
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
        step.isSynth = stepConfig.hasDegree;
        if (step.isSynth) {
            step.frequencyHz = Theory::DegreeToFrequency(root, scale, stepConfig.degree);
        }
        pattern.steps.push_back(step);
    }

    std::stable_sort(pattern.steps.begin(), pattern.steps.end(),
                      [](const ResolvedStep& a, const ResolvedStep& b) { return a.beatOffset < b.beatOffset; });

    return pattern;
}

} // namespace pmg
