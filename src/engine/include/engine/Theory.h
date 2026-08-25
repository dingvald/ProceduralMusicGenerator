#pragma once

#include <string>
#include <vector>

namespace pmg {

enum class NoteName { C, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B };

// Scale/mode used to constrain procedurally-generated melodies (see
// MelodyGenerator) to a musical key. Independent of NoteName-based absolute
// pitch, which stays the only mechanism for hand-authored steps/melodies.
enum class Scale { Major, NaturalMinor, HarmonicMinor, Dorian, Mixolydian, MajorPentatonic, MinorPentatonic, Blues };

// Static note-name/scale -> frequency helpers. Used once at pattern-build
// time (ConfigLoader/Pattern/NoteStringParser/MelodyGenerator), not per-note
// at runtime.
class Theory {
public:
    static NoteName ParseNoteName(const std::string& name);
    static Scale ParseScale(const std::string& name);

    // Absolute pitch: octave 4 contains MIDI 60 (C4). Standard equal
    // temperament, A4 == MIDI 69 == 440 Hz.
    static float NoteToFrequency(NoteName note, int octave = 4);

    // This scale's semitone offsets from its root, ascending within one
    // octave (e.g. Major == {0,2,4,5,7,9,11}). Always starts at 0.
    static const std::vector<int>& ScaleIntervals(Scale scale);

    // Resolves a scale degree (0 == root; negative or >= ScaleIntervals(scale).size()
    // wraps into neighboring octaves, e.g. degree == intervals.size() is the
    // root one octave up) against `root`/`scale`, anchored at `baseOctave`
    // (the octave `root` itself would use for degree 0).
    static void DegreeToNote(NoteName root, Scale scale, int degree, int baseOctave, NoteName& outNote,
                              int& outOctave);
};

} // namespace pmg
