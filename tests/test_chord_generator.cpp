#include <stdexcept>

#include "doctest/doctest.h"
#include "engine/ChordGenerator.h"

using namespace pmg;

namespace {

GeneratedChordConfig MakeConfig() {
    GeneratedChordConfig config;
    config.instrument = "pad";
    config.key = NoteName::C;
    config.scale = Scale::Major;
    config.baseOctave = 3;
    config.degrees = {0, 3, 4, 0};
    config.chordLengthBeats = 4.0;
    config.seventh = false;
    config.velocity = 0.7f;
    config.gateFraction = 0.9f;
    return config;
}

} // namespace

TEST_CASE("GenerateChords: a triad emits exactly 3 same-beat steps per degree") {
    GeneratedChordConfig config = MakeConfig();
    std::vector<StepConfig> steps = GenerateChords(config);

    REQUIRE(steps.size() == config.degrees.size() * 3);
    for (size_t i = 0; i < config.degrees.size(); ++i) {
        for (size_t tone = 0; tone < 3; ++tone) {
            const StepConfig& step = steps[i * 3 + tone];
            CHECK(step.beat == doctest::Approx(static_cast<double>(i) * config.chordLengthBeats));
            CHECK(step.instrument == "pad");
            CHECK(step.hasNote);
        }
    }
}

TEST_CASE("GenerateChords: seventh == true emits a 4th tone per degree") {
    GeneratedChordConfig config = MakeConfig();
    config.seventh = true;
    std::vector<StepConfig> steps = GenerateChords(config);

    CHECK(steps.size() == config.degrees.size() * 4);
}

TEST_CASE("GenerateChords: the root chord (degree 0, C major) is C-E-G") {
    GeneratedChordConfig config = MakeConfig();
    config.degrees = {0};
    std::vector<StepConfig> steps = GenerateChords(config);

    REQUIRE(steps.size() == 3);
    CHECK(steps[0].note == NoteName::C);
    CHECK(steps[0].octave == 3);
    CHECK(steps[1].note == NoteName::E);
    CHECK(steps[2].note == NoteName::G);
}

TEST_CASE("GenerateChords: gate/velocity propagate from config") {
    GeneratedChordConfig config = MakeConfig();
    std::vector<StepConfig> steps = GenerateChords(config);

    for (const StepConfig& step : steps) {
        CHECK(step.velocity == doctest::Approx(config.velocity));
        CHECK(step.gate == doctest::Approx(config.chordLengthBeats * config.gateFraction));
    }
}

TEST_CASE("GenerateChords throws on empty degrees or non-positive chordLengthBeats") {
    GeneratedChordConfig emptyDegrees = MakeConfig();
    emptyDegrees.degrees.clear();
    CHECK_THROWS_AS(GenerateChords(emptyDegrees), std::runtime_error);

    GeneratedChordConfig badLength = MakeConfig();
    badLength.chordLengthBeats = 0.0;
    CHECK_THROWS_AS(GenerateChords(badLength), std::runtime_error);
}
