#include "engine/ChordGenerator.h"

#include <stdexcept>

#include "engine/Theory.h"

namespace pmg {

namespace {

// Root/third/fifth, in scale-degree offsets from the progression entry's
// degree; kSeventhOffset is appended on top when config.seventh is set.
constexpr int kTriadOffsets[] = {0, 2, 4};
constexpr int kSeventhOffset = 6;

} // namespace

std::vector<StepConfig> GenerateChords(const GeneratedChordConfig& config) {
    if (config.degrees.empty()) {
        throw std::runtime_error("ChordGenerator: degrees must be non-empty");
    }
    if (config.chordLengthBeats <= 0.0) {
        throw std::runtime_error("ChordGenerator: chordLengthBeats must be positive");
    }

    std::vector<StepConfig> steps;
    float gate = static_cast<float>(config.chordLengthBeats * config.gateFraction);

    for (size_t i = 0; i < config.degrees.size(); ++i) {
        double beat = static_cast<double>(i) * config.chordLengthBeats;
        int degree = config.degrees[i];

        for (int offset : kTriadOffsets) {
            NoteName note;
            int octave;
            Theory::DegreeToNote(config.key, config.scale, degree + offset, config.baseOctave, note, octave);

            StepConfig step;
            step.beat = beat;
            step.instrument = config.instrument;
            step.hasNote = true;
            step.note = note;
            step.octave = octave;
            step.velocity = config.velocity;
            step.gate = gate;
            steps.push_back(step);
        }

        if (config.seventh) {
            NoteName note;
            int octave;
            Theory::DegreeToNote(config.key, config.scale, degree + kSeventhOffset, config.baseOctave, note, octave);

            StepConfig step;
            step.beat = beat;
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
