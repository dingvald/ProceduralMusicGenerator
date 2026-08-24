#include "engine/Theory.h"

#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace pmg {

namespace {

const std::vector<int>& ScaleIntervals(ScaleType scale) {
    static const std::vector<int> kMajor = {0, 2, 4, 5, 7, 9, 11};
    static const std::vector<int> kNaturalMinor = {0, 2, 3, 5, 7, 8, 10};
    static const std::vector<int> kMinorPentatonic = {0, 3, 5, 7, 10};
    static const std::vector<int> kMajorPentatonic = {0, 2, 4, 7, 9};
    static const std::vector<int> kChromatic = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    switch (scale) {
        case ScaleType::Major: return kMajor;
        case ScaleType::NaturalMinor: return kNaturalMinor;
        case ScaleType::MinorPentatonic: return kMinorPentatonic;
        case ScaleType::MajorPentatonic: return kMajorPentatonic;
        case ScaleType::Chromatic: return kChromatic;
    }
    return kMajor;
}

} // namespace

NoteName Theory::ParseNoteName(const std::string& name) {
    static const std::unordered_map<std::string, NoteName> kNames = {
        {"C", NoteName::C},
        {"C#", NoteName::Cs}, {"Db", NoteName::Cs},
        {"D", NoteName::D},
        {"D#", NoteName::Ds}, {"Eb", NoteName::Ds},
        {"E", NoteName::E},
        {"F", NoteName::F},
        {"F#", NoteName::Fs}, {"Gb", NoteName::Fs},
        {"G", NoteName::G},
        {"G#", NoteName::Gs}, {"Ab", NoteName::Gs},
        {"A", NoteName::A},
        {"A#", NoteName::As}, {"Bb", NoteName::As},
        {"B", NoteName::B},
    };

    auto it = kNames.find(name);
    if (it == kNames.end()) {
        throw std::runtime_error("Theory::ParseNoteName: unknown note name '" + name + "'");
    }
    return it->second;
}

ScaleType Theory::ParseScaleType(const std::string& name) {
    static const std::unordered_map<std::string, ScaleType> kScales = {
        {"major", ScaleType::Major},
        {"natural_minor", ScaleType::NaturalMinor},
        {"minor_pentatonic", ScaleType::MinorPentatonic},
        {"major_pentatonic", ScaleType::MajorPentatonic},
        {"chromatic", ScaleType::Chromatic},
    };

    auto it = kScales.find(name);
    if (it == kScales.end()) {
        throw std::runtime_error("Theory::ParseScaleType: unknown scale type '" + name + "'");
    }
    return it->second;
}

float Theory::DegreeToFrequency(NoteName root, ScaleType scale, int degree, int octave) {
    const std::vector<int>& intervals = ScaleIntervals(scale);
    int scaleLen = static_cast<int>(intervals.size());

    int octaveOffset = degree >= 0 ? degree / scaleLen : -((-degree + scaleLen - 1) / scaleLen);
    int scaleIndex = degree - octaveOffset * scaleLen;
    if (scaleIndex < 0 || scaleIndex >= scaleLen) {
        // Defensive; the formula above should already keep this in range.
        scaleIndex = ((scaleIndex % scaleLen) + scaleLen) % scaleLen;
    }

    int rootIndex = static_cast<int>(root);
    int midiRoot = (octave + 1) * 12 + rootIndex; // C4 == MIDI 60
    int semitoneFromRoot = intervals[scaleIndex] + 12 * octaveOffset;
    int midiNote = midiRoot + semitoneFromRoot;

    return 440.0f * std::pow(2.0f, static_cast<float>(midiNote - 69) / 12.0f); // A4 == MIDI 69 == 440 Hz
}

} // namespace pmg
