#pragma once

#include <string>

namespace pmg {

enum class NoteName { C, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B };

enum class ScaleType { Major, NaturalMinor, MinorPentatonic, MajorPentatonic, Chromatic };

// Static scale-degree -> frequency helper. Used once at pattern-build time
// (ConfigLoader/Pattern), not per-note at runtime.
class Theory {
public:
    static NoteName ParseNoteName(const std::string& name);
    static ScaleType ParseScaleType(const std::string& name);

    // degree 0 = root at the given octave; degree may be negative or exceed
    // the scale's length and wraps to lower/higher octaves accordingly
    // (e.g. degree 5 in a 5-note pentatonic scale is the root one octave up).
    static float DegreeToFrequency(NoteName root, ScaleType scale, int degree, int octave = 4);
};

} // namespace pmg
