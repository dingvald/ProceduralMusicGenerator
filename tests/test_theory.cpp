#include <stdexcept>

#include "doctest/doctest.h"
#include "engine/Theory.h"

using namespace pmg;

TEST_CASE("Theory::ParseNoteName maps sharps and flats to the same NoteName") {
    CHECK(Theory::ParseNoteName("C#") == Theory::ParseNoteName("Db"));
    CHECK(Theory::ParseNoteName("D#") == Theory::ParseNoteName("Eb"));
    CHECK(Theory::ParseNoteName("F#") == Theory::ParseNoteName("Gb"));
    CHECK(Theory::ParseNoteName("G#") == Theory::ParseNoteName("Ab"));
    CHECK(Theory::ParseNoteName("A#") == Theory::ParseNoteName("Bb"));
}

TEST_CASE("Theory::ParseNoteName throws on an unknown note name") {
    CHECK_THROWS_AS(Theory::ParseNoteName("H"), std::runtime_error);
    CHECK_THROWS_AS(Theory::ParseNoteName(""), std::runtime_error);
    CHECK_THROWS_AS(Theory::ParseNoteName("Cb"), std::runtime_error); // not in the lookup table
}

TEST_CASE("Theory::ParseNoteName is case-sensitive") {
    // Documents a contract callers (e.g. NoteStringParser) must normalize
    // around: only capital-letter, lowercase-accidental spellings are known.
    CHECK_THROWS_AS(Theory::ParseNoteName("c"), std::runtime_error);
    CHECK_THROWS_AS(Theory::ParseNoteName("a#"), std::runtime_error);
    CHECK_NOTHROW(Theory::ParseNoteName("A#"));
}

TEST_CASE("Theory::NoteToFrequency: A4 is exactly 440Hz") {
    CHECK(Theory::NoteToFrequency(NoteName::A, 4) == doctest::Approx(440.0f));
}

TEST_CASE("Theory::NoteToFrequency: C4 matches the standard equal-tempered reference value") {
    CHECK(Theory::NoteToFrequency(NoteName::C, 4) == doctest::Approx(261.6256f).epsilon(0.001));
}

TEST_CASE("Theory::NoteToFrequency: all 12 pitch classes at octave 4 match equal temperament") {
    CHECK(Theory::NoteToFrequency(NoteName::C, 4) == doctest::Approx(261.63f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::Cs, 4) == doctest::Approx(277.18f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::D, 4) == doctest::Approx(293.66f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::Ds, 4) == doctest::Approx(311.13f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::E, 4) == doctest::Approx(329.63f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::F, 4) == doctest::Approx(349.23f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::Fs, 4) == doctest::Approx(369.99f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::G, 4) == doctest::Approx(392.00f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::Gs, 4) == doctest::Approx(415.30f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::A, 4) == doctest::Approx(440.00f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::As, 4) == doctest::Approx(466.16f).epsilon(0.001));
    CHECK(Theory::NoteToFrequency(NoteName::B, 4) == doctest::Approx(493.88f).epsilon(0.001));
}

TEST_CASE("Theory::NoteToFrequency: moving up an octave doubles frequency") {
    float a3 = Theory::NoteToFrequency(NoteName::A, 3);
    float a4 = Theory::NoteToFrequency(NoteName::A, 4);
    float a5 = Theory::NoteToFrequency(NoteName::A, 5);

    CHECK(a4 == doctest::Approx(a3 * 2.0f).epsilon(0.0001));
    CHECK(a5 == doctest::Approx(a4 * 2.0f).epsilon(0.0001));
}

TEST_CASE("Theory::NoteToFrequency: default octave is 4") {
    CHECK(Theory::NoteToFrequency(NoteName::A) == doctest::Approx(Theory::NoteToFrequency(NoteName::A, 4)));
}

TEST_CASE("Theory::NoteToFrequency handles extreme octaves without crashing, monotonically") {
    float low = Theory::NoteToFrequency(NoteName::C, 0);
    float high = Theory::NoteToFrequency(NoteName::C, 9);
    CHECK(low > 0.0f);
    CHECK(high > low);
}

TEST_CASE("Theory::ParseScale round-trips every known scale name") {
    CHECK(Theory::ParseScale("major") == Scale::Major);
    CHECK(Theory::ParseScale("naturalMinor") == Scale::NaturalMinor);
    CHECK(Theory::ParseScale("harmonicMinor") == Scale::HarmonicMinor);
    CHECK(Theory::ParseScale("dorian") == Scale::Dorian);
    CHECK(Theory::ParseScale("mixolydian") == Scale::Mixolydian);
    CHECK(Theory::ParseScale("majorPentatonic") == Scale::MajorPentatonic);
    CHECK(Theory::ParseScale("minorPentatonic") == Scale::MinorPentatonic);
    CHECK(Theory::ParseScale("blues") == Scale::Blues);
}

TEST_CASE("Theory::ParseScale throws on an unknown scale name") {
    CHECK_THROWS_AS(Theory::ParseScale("phrygian"), std::runtime_error);
    CHECK_THROWS_AS(Theory::ParseScale(""), std::runtime_error);
}

TEST_CASE("Theory::ScaleIntervals: every scale starts on the root (interval 0)") {
    CHECK(Theory::ScaleIntervals(Scale::Major).front() == 0);
    CHECK(Theory::ScaleIntervals(Scale::NaturalMinor).front() == 0);
    CHECK(Theory::ScaleIntervals(Scale::MajorPentatonic).front() == 0);
    CHECK(Theory::ScaleIntervals(Scale::Blues).front() == 0);
}

TEST_CASE("Theory::ScaleIntervals: Major matches the standard whole/half-step pattern") {
    CHECK(Theory::ScaleIntervals(Scale::Major) == std::vector<int>{0, 2, 4, 5, 7, 9, 11});
}

TEST_CASE("Theory::ScaleIntervals: pentatonic scales have exactly 5 degrees") {
    CHECK(Theory::ScaleIntervals(Scale::MajorPentatonic).size() == 5);
    CHECK(Theory::ScaleIntervals(Scale::MinorPentatonic).size() == 5);
}

TEST_CASE("Theory::DegreeToNote: degree 0 is exactly the root, at baseOctave") {
    NoteName note;
    int octave;
    Theory::DegreeToNote(NoteName::A, Scale::MajorPentatonic, 0, 4, note, octave);
    CHECK(note == NoteName::A);
    CHECK(octave == 4);
}

TEST_CASE("Theory::DegreeToNote: C major degree 1 is D4, degree 7 wraps to C5") {
    NoteName note;
    int octave;

    Theory::DegreeToNote(NoteName::C, Scale::Major, 1, 4, note, octave);
    CHECK(note == NoteName::D);
    CHECK(octave == 4);

    Theory::DegreeToNote(NoteName::C, Scale::Major, 7, 4, note, octave);
    CHECK(note == NoteName::C);
    CHECK(octave == 5);
}

TEST_CASE("Theory::DegreeToNote: negative degrees wrap down into the octave below") {
    NoteName note;
    int octave;

    // C major degree -1 is the scale's 7th degree (B) one octave down.
    Theory::DegreeToNote(NoteName::C, Scale::Major, -1, 4, note, octave);
    CHECK(note == NoteName::B);
    CHECK(octave == 3);
}

TEST_CASE("Theory::DegreeToNote: frequency rises monotonically as degree increases") {
    NoteName prevNote;
    int prevOctave;
    Theory::DegreeToNote(NoteName::A, Scale::MinorPentatonic, 0, 4, prevNote, prevOctave);
    float prevFreq = Theory::NoteToFrequency(prevNote, prevOctave);

    for (int degree = 1; degree <= 10; ++degree) {
        NoteName note;
        int octave;
        Theory::DegreeToNote(NoteName::A, Scale::MinorPentatonic, degree, 4, note, octave);
        float freq = Theory::NoteToFrequency(note, octave);
        CHECK(freq > prevFreq);
        prevFreq = freq;
    }
}
