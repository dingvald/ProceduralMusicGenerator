#include "engine/Theory.h"

#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace pmg {

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

} // namespace pmg
