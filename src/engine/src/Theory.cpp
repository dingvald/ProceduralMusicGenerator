#include "engine/Theory.h"

#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace pmg {

namespace {

// Floor division/modulo (as opposed to C++'s truncating %), needed so
// negative scale degrees wrap into the octave below rather than toward zero.
int FloorDiv(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        --q;
    }
    return q;
}

int FloorMod(int a, int b) {
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        r += b;
    }
    return r;
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

float Theory::NoteToFrequency(NoteName note, int octave) {
    int midiNote = (octave + 1) * 12 + static_cast<int>(note); // C4 == MIDI 60
    return 440.0f * std::pow(2.0f, static_cast<float>(midiNote - 69) / 12.0f); // A4 == MIDI 69 == 440 Hz
}

Scale Theory::ParseScale(const std::string& name) {
    static const std::unordered_map<std::string, Scale> kScales = {
        {"major", Scale::Major},
        {"naturalMinor", Scale::NaturalMinor},
        {"harmonicMinor", Scale::HarmonicMinor},
        {"dorian", Scale::Dorian},
        {"mixolydian", Scale::Mixolydian},
        {"majorPentatonic", Scale::MajorPentatonic},
        {"minorPentatonic", Scale::MinorPentatonic},
        {"blues", Scale::Blues},
    };

    auto it = kScales.find(name);
    if (it == kScales.end()) {
        throw std::runtime_error("Theory::ParseScale: unknown scale '" + name + "'");
    }
    return it->second;
}

const std::vector<int>& Theory::ScaleIntervals(Scale scale) {
    static const std::vector<int> kMajor = {0, 2, 4, 5, 7, 9, 11};
    static const std::vector<int> kNaturalMinor = {0, 2, 3, 5, 7, 8, 10};
    static const std::vector<int> kHarmonicMinor = {0, 2, 3, 5, 7, 8, 11};
    static const std::vector<int> kDorian = {0, 2, 3, 5, 7, 9, 10};
    static const std::vector<int> kMixolydian = {0, 2, 4, 5, 7, 9, 10};
    static const std::vector<int> kMajorPentatonic = {0, 2, 4, 7, 9};
    static const std::vector<int> kMinorPentatonic = {0, 3, 5, 7, 10};
    static const std::vector<int> kBlues = {0, 3, 5, 6, 7, 10};

    switch (scale) {
        case Scale::Major: return kMajor;
        case Scale::NaturalMinor: return kNaturalMinor;
        case Scale::HarmonicMinor: return kHarmonicMinor;
        case Scale::Dorian: return kDorian;
        case Scale::Mixolydian: return kMixolydian;
        case Scale::MajorPentatonic: return kMajorPentatonic;
        case Scale::MinorPentatonic: return kMinorPentatonic;
        case Scale::Blues: return kBlues;
    }
    throw std::runtime_error("Theory::ScaleIntervals: unhandled Scale enumerator");
}

void Theory::DegreeToNote(NoteName root, Scale scale, int degree, int baseOctave, NoteName& outNote,
                           int& outOctave) {
    const std::vector<int>& intervals = ScaleIntervals(scale);
    int degreeCount = static_cast<int>(intervals.size());

    int octaveShift = FloorDiv(degree, degreeCount);
    int degreeInScale = FloorMod(degree, degreeCount);

    int absoluteSemitone = static_cast<int>(root) + intervals[static_cast<size_t>(degreeInScale)] + octaveShift * 12;

    outOctave = baseOctave + FloorDiv(absoluteSemitone, 12);
    outNote = static_cast<NoteName>(FloorMod(absoluteSemitone, 12));
}

} // namespace pmg
