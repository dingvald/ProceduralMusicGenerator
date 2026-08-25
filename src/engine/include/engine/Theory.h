#pragma once

#include <string>

namespace pmg {

enum class NoteName { C, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B };

// Static note-name -> frequency helper. Used once at pattern-build time
// (ConfigLoader/Pattern/NoteStringParser), not per-note at runtime.
class Theory {
public:
    static NoteName ParseNoteName(const std::string& name);

    // Absolute pitch: octave 4 contains MIDI 60 (C4). Standard equal
    // temperament, A4 == MIDI 69 == 440 Hz.
    static float NoteToFrequency(NoteName note, int octave = 4);
};

} // namespace pmg
