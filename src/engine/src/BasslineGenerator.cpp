#include "engine/BasslineGenerator.h"

#include <cmath>
#include <stdexcept>

#include "engine/Theory.h"

namespace pmg {

namespace {

// Chord-tone scale-degree offsets a passing-tone slot can land on, weighted
// toward staying on the root -- mirrors MelodyGenerator's small, explicit
// weighted-delta table but constrained to the current chord's own tones
// instead of an unconstrained scale walk.
constexpr int kChordToneOffsets[] = {0, 2, 4}; // root, third, fifth
constexpr float kChordToneWeights[] = {0.65f, 0.20f, 0.15f};

} // namespace

std::vector<StepConfig> GenerateBassline(const GeneratedBasslineConfig& config, RandomSource& rng) {
    if (config.degrees.empty()) {
        throw std::runtime_error("BasslineGenerator: degrees must be non-empty");
    }
    if (config.chordLengthBeats <= 0.0 || config.noteLengthBeats <= 0.0) {
        throw std::runtime_error("BasslineGenerator: chordLengthBeats and noteLengthBeats must both be positive");
    }

    int slotsPerSpan = static_cast<int>(std::llround(config.chordLengthBeats / config.noteLengthBeats));
    if (slotsPerSpan <= 0) {
        slotsPerSpan = 1;
    }
    float gate = static_cast<float>(config.noteLengthBeats * config.gateFraction);

    std::vector<StepConfig> steps;
    for (size_t i = 0; i < config.degrees.size(); ++i) {
        double spanStartBeat = static_cast<double>(i) * config.chordLengthBeats;

        for (int slot = 0; slot < slotsPerSpan; ++slot) {
            int offset = 0;
            if (slot > 0 && rng.NextBernoulli(config.passingToneProbability)) {
                std::vector<float> weights(std::begin(kChordToneWeights), std::end(kChordToneWeights));
                int chosen = rng.NextWeightedIndex(weights);
                if (chosen >= 0) {
                    offset = kChordToneOffsets[static_cast<size_t>(chosen)];
                }
            }

            NoteName note;
            int octave;
            Theory::DegreeToNote(config.key, config.scale, config.degrees[i] + offset, config.baseOctave, note,
                                  octave);

            StepConfig step;
            step.beat = spanStartBeat + slot * config.noteLengthBeats;
            step.instrument = config.instrument;
            step.hasNote = true;
            step.note = note;
            step.octave = octave;
            step.velocity = config.velocity;
            step.gate = gate;
            steps.push_back(step);
        }
    }

    return steps;
}

} // namespace pmg
