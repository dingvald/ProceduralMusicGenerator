#include <stdexcept>

#include "doctest/doctest.h"
#include "engine/NoteStringParser.h"

using namespace pmg;

namespace {

MelodyConfig MakeMelody(const std::string& notes) {
    MelodyConfig melody;
    melody.instrument = "lead";
    melody.notes = notes;
    return melody;
}

} // namespace

TEST_CASE("ParseNoteString: the user's example, 'A B G F#'") {
    std::vector<StepConfig> steps = ParseNoteString(MakeMelody("A B G F#"));

    REQUIRE(steps.size() == 4);
    NoteName expected[] = {NoteName::A, NoteName::B, NoteName::G, NoteName::Fs};
    for (size_t i = 0; i < steps.size(); ++i) {
        CHECK(steps[i].hasNote);
        CHECK(steps[i].note == expected[i]);
        CHECK(steps[i].octave == 4); // MelodyConfig::defaultOctave
        CHECK(steps[i].beat == doctest::Approx(static_cast<double>(i))); // noteLengthBeats == 1.0
        CHECK(steps[i].instrument == "lead");
    }
}

TEST_CASE("ParseNoteString is case-insensitive") {
    std::vector<StepConfig> upper = ParseNoteString(MakeMelody("A B G F#"));
    std::vector<StepConfig> lower = ParseNoteString(MakeMelody("a b g f#"));

    REQUIRE(upper.size() == lower.size());
    for (size_t i = 0; i < upper.size(); ++i) {
        CHECK(upper[i].note == lower[i].note);
    }
}

TEST_CASE("ParseNoteString handles flats, cross-checked against Theory::ParseNoteName") {
    std::vector<StepConfig> steps = ParseNoteString(MakeMelody("Bb Eb"));
    REQUIRE(steps.size() == 2);
    CHECK(steps[0].note == Theory::ParseNoteName("Bb"));
    CHECK(steps[1].note == Theory::ParseNoteName("Eb"));
}

TEST_CASE("ParseNoteString: explicit per-token octave overrides defaultOctave") {
    std::vector<StepConfig> steps = ParseNoteString(MakeMelody("C3 C4 C5"));
    REQUIRE(steps.size() == 3);
    CHECK(steps[0].octave == 3);
    CHECK(steps[1].octave == 4);
    CHECK(steps[2].octave == 5);
    // cursor still advances uniformly regardless of the octave override
    CHECK(steps[1].beat == doctest::Approx(1.0));
    CHECK(steps[2].beat == doctest::Approx(2.0));
}

TEST_CASE("ParseNoteString: melody.defaultOctave is used when no digit is given") {
    MelodyConfig melody = MakeMelody("A");
    melody.defaultOctave = 6;
    std::vector<StepConfig> steps = ParseNoteString(melody);
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].octave == 6);
}

TEST_CASE("ParseNoteString: rests advance the cursor without emitting a step") {
    std::vector<StepConfig> steps = ParseNoteString(MakeMelody("A R B"));
    REQUIRE(steps.size() == 2);
    CHECK(steps[0].note == NoteName::A);
    CHECK(steps[0].beat == doctest::Approx(0.0));
    CHECK(steps[1].note == NoteName::B);
    CHECK(steps[1].beat == doctest::Approx(2.0)); // rest at beat 1.0 consumed a full slot
}

TEST_CASE("ParseNoteString: consecutive rests each advance the cursor") {
    std::vector<StepConfig> steps = ParseNoteString(MakeMelody("A R R B"));
    REQUIRE(steps.size() == 2);
    CHECK(steps[1].beat == doctest::Approx(3.0));
}

TEST_CASE("ParseNoteString: a rest accepts a duration override") {
    std::vector<StepConfig> steps = ParseNoteString(MakeMelody("R:2 A"));
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].beat == doctest::Approx(2.0));
}

TEST_CASE("ParseNoteString: per-token duration override changes cursor advance and gate") {
    MelodyConfig melody = MakeMelody("A:2 B");
    melody.gateFraction = 0.5f;
    std::vector<StepConfig> steps = ParseNoteString(melody);

    REQUIRE(steps.size() == 2);
    CHECK(steps[0].beat == doctest::Approx(0.0));
    CHECK(steps[0].gate == doctest::Approx(1.0f)); // 2 beats * 0.5 gateFraction
    CHECK(steps[1].beat == doctest::Approx(2.0));  // B starts after A's 2-beat slot, not 1
}

TEST_CASE("ParseNoteString: default gate is noteLengthBeats * gateFraction") {
    MelodyConfig melody = MakeMelody("A");
    melody.noteLengthBeats = 1.5;
    melody.gateFraction = 0.8f;
    std::vector<StepConfig> steps = ParseNoteString(melody);
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].gate == doctest::Approx(1.2f));
}

TEST_CASE("ParseNoteString: whitespace variants normalize identically") {
    std::vector<StepConfig> spaces = ParseNoteString(MakeMelody("A B G"));
    std::vector<StepConfig> mixed = ParseNoteString(MakeMelody("  A\tB\n\nG  "));

    REQUIRE(spaces.size() == mixed.size());
    for (size_t i = 0; i < spaces.size(); ++i) {
        CHECK(spaces[i].note == mixed[i].note);
        CHECK(spaces[i].beat == doctest::Approx(mixed[i].beat));
    }
}

TEST_CASE("ParseNoteString: empty or whitespace-only strings are legal and produce no steps") {
    CHECK(ParseNoteString(MakeMelody("")).empty());
    CHECK(ParseNoteString(MakeMelody("   \t\n ")).empty());
}

TEST_CASE("ParseNoteString: instrument and velocity pass through from MelodyConfig") {
    MelodyConfig melody = MakeMelody("A B");
    melody.instrument = "bass";
    melody.velocity = 0.42f;
    std::vector<StepConfig> steps = ParseNoteString(melody);
    for (const StepConfig& step : steps) {
        CHECK(step.instrument == "bass");
        CHECK(step.velocity == doctest::Approx(0.42f));
    }
}

TEST_CASE("ParseNoteString: startBeat offsets the whole melody") {
    MelodyConfig melody = MakeMelody("A B");
    melody.startBeat = 4.0;
    std::vector<StepConfig> steps = ParseNoteString(melody);
    REQUIRE(steps.size() == 2);
    CHECK(steps[0].beat == doctest::Approx(4.0));
    CHECK(steps[1].beat == doctest::Approx(5.0));
}

TEST_CASE("ParseNoteString throws on malformed input") {
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("H")), std::runtime_error);           // unknown letter
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("3")), std::runtime_error);           // digit with no letter
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("A##")), std::runtime_error);         // double accidental
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("aB3")), std::runtime_error);         // uppercase B as flat
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("A4x")), std::runtime_error);         // trailing garbage
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("R#")), std::runtime_error);          // rest carrying pitch
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("R3")), std::runtime_error);          // rest carrying octave
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("A:")), std::runtime_error);          // empty duration
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("A:abc")), std::runtime_error);       // malformed duration
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("A:0")), std::runtime_error);         // non-positive duration
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("A:-1")), std::runtime_error);        // negative duration
    CHECK_THROWS_AS(ParseNoteString(MakeMelody(":2")), std::runtime_error);          // bare duration, no note
    CHECK_THROWS_AS(ParseNoteString(MakeMelody("Cb")), std::runtime_error);          // not in Theory's table
}

TEST_CASE("ParseNoteString error messages name the offending token") {
    try {
        ParseNoteString(MakeMelody("C D A## F"));
        FAIL("expected std::runtime_error");
    } catch (const std::runtime_error& e) {
        std::string message = e.what();
        CHECK(message.find("A##") != std::string::npos);
    }
}
