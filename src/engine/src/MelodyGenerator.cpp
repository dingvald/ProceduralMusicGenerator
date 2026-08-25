#include "engine/MelodyGenerator.h"

#include <cmath>
#include <iterator>
#include <stdexcept>

#include "engine/Theory.h"

namespace pmg {

namespace {

// Scale-degree deltas a step can move by, weighted toward small movement so
// generated melodies have a smooth, singable contour instead of jumping
// around at random. Index 3 (delta 0) is the "stay put" option.
constexpr int kDeltas[] = {-3, -2, -1, 0, 1, 2, 3};
constexpr float kDeltaWeights[] = {0.05f, 0.15f, 0.25f, 0.10f, 0.25f, 0.15f, 0.05f};
constexpr size_t kDeltaCount = std::size(kDeltas);

} // namespace

std::vector<StepConfig> GenerateMelody(const GeneratedMelodyConfig& config, RandomSource& rng) {
    if (config.lengthBeats <= 0.0 || config.noteLengthBeats <= 0.0) {
        throw std::runtime_error("MelodyGenerator: lengthBeats and noteLengthBeats must both be positive");
    }
    if (config.octaveRange < 0) {
        throw std::runtime_error("MelodyGenerator: octaveRange must be >= 0");
    }

    int slotCount = static_cast<int>(std::llround(config.lengthBeats / config.noteLengthBeats));
    if (slotCount <= 0) {
        slotCount = 1;
    }

    std::vector<StepConfig> steps;
    int currentDegree = 0; // starts on the root -- resolves to exactly baseOctave, see Theory::DegreeToNote

    for (int slot = 0; slot < slotCount; ++slot) {
        double beat = slot * config.noteLengthBeats;

        if (rng.NextBernoulli(config.restProbability)) {
            continue;
        }

        // Zero out any delta that would walk outside the configured octave
        // range so every candidate NextWeightedIndex can pick is already
        // valid -- delta 0 (stay on the current, already-valid degree) is
        // always a legal fallback.
        std::vector<float> candidateWeights(std::begin(kDeltaWeights), std::end(kDeltaWeights));
        for (size_t i = 0; i < kDeltaCount; ++i) {
            NoteName candidateNote;
            int candidateOctave;
            Theory::DegreeToNote(config.key, config.scale, currentDegree + kDeltas[i], config.baseOctave,
                                  candidateNote, candidateOctave);
            if (candidateOctave < config.baseOctave || candidateOctave > config.baseOctave + config.octaveRange) {
                candidateWeights[i] = 0.0f;
            }
        }

        int chosen = rng.NextWeightedIndex(candidateWeights);
        if (chosen < 0) {
            chosen = 3; // delta 0 -- defensive fallback, unreachable given the invariant above
        }
        currentDegree += kDeltas[static_cast<size_t>(chosen)];

        NoteName note;
        int octave;
        Theory::DegreeToNote(config.key, config.scale, currentDegree, config.baseOctave, note, octave);

        StepConfig step;
        step.beat = beat;
        step.instrument = config.instrument;
        step.hasNote = true;
        step.note = note;
        step.octave = octave;
        step.velocity = config.velocity;
        step.gate = static_cast<float>(config.noteLengthBeats * config.gateFraction);
        steps.push_back(step);
    }

    return steps;
}

} // namespace pmg
