#include "engine/RhythmGenerator.h"

#include <algorithm>
#include <stdexcept>

namespace pmg {

namespace {

// Classic two-list Bjorklund construction: start with `pulses` singleton
// groups of [true] and `steps - pulses` singleton groups of [false], then
// repeatedly pair off the front of each list (append each tail group onto
// the matching head group) until at most one tail group remains.
// Concatenating the resulting groups gives the Euclidean rhythm. Terminates
// like the subtractive form of the Euclidean GCD algorithm -- the tail
// list's size strictly shrinks each iteration.
std::vector<bool> Bjorklund(int pulses, int steps) {
    if (pulses <= 0) {
        return std::vector<bool>(static_cast<size_t>(steps), false);
    }
    if (pulses >= steps) {
        return std::vector<bool>(static_cast<size_t>(steps), true);
    }

    std::vector<std::vector<bool>> head(static_cast<size_t>(pulses), std::vector<bool>{true});
    std::vector<std::vector<bool>> tail(static_cast<size_t>(steps - pulses), std::vector<bool>{false});

    while (tail.size() > 1) {
        size_t n = std::min(head.size(), tail.size());

        std::vector<std::vector<bool>> merged;
        merged.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            std::vector<bool> group = head[i];
            group.insert(group.end(), tail[i].begin(), tail[i].end());
            merged.push_back(std::move(group));
        }

        std::vector<std::vector<bool>> remainder =
            (head.size() > n) ? std::vector<std::vector<bool>>(head.begin() + static_cast<long>(n), head.end())
                               : std::vector<std::vector<bool>>(tail.begin() + static_cast<long>(n), tail.end());

        head = std::move(merged);
        tail = std::move(remainder);
    }

    std::vector<bool> result;
    for (const auto& group : head) {
        result.insert(result.end(), group.begin(), group.end());
    }
    for (const auto& group : tail) {
        result.insert(result.end(), group.begin(), group.end());
    }
    return result;
}

} // namespace

std::vector<StepConfig> GenerateRhythm(const GeneratedRhythmConfig& config, RandomSource& rng) {
    if (config.steps <= 0) {
        throw std::runtime_error("RhythmGenerator: steps must be positive");
    }
    if (config.lengthBeats <= 0.0) {
        throw std::runtime_error("RhythmGenerator: lengthBeats must be positive");
    }
    if (config.velocityJitter < 0.0f) {
        throw std::runtime_error("RhythmGenerator: velocityJitter must be >= 0");
    }

    std::vector<bool> pattern = Bjorklund(config.pulses, config.steps);
    double stepBeats = config.lengthBeats / config.steps;

    std::vector<StepConfig> steps;
    for (int i = 0; i < config.steps; ++i) {
        if (!pattern[static_cast<size_t>(i)]) {
            continue;
        }

        float velocity = config.velocity;
        if (config.velocityJitter > 0.0f) {
            double jitter = (rng.NextUniform01() * 2.0 - 1.0) * config.velocityJitter;
            velocity = std::clamp(config.velocity + static_cast<float>(jitter), 0.0f, 1.0f);
        }

        StepConfig step;
        step.beat = i * stepBeats;
        step.instrument = config.instrument;
        step.hasNote = config.hasNote;
        step.note = config.note;
        step.octave = config.octave;
        step.velocity = velocity;
        step.gate = config.gate;
        steps.push_back(step);
    }

    return steps;
}

} // namespace pmg
