#include <cmath>
#include <stdexcept>

#include "doctest/doctest.h"
#include "engine/BasslineGenerator.h"
#include "engine/RandomSource.h"

using namespace pmg;

namespace {

GeneratedBasslineConfig MakeConfig() {
    GeneratedBasslineConfig config;
    config.instrument = "bass";
    config.key = NoteName::C;
    config.scale = Scale::Major;
    config.baseOctave = 2;
    config.degrees = {0, 3, 4, 0};
    config.chordLengthBeats = 4.0;
    config.noteLengthBeats = 1.0;
    config.passingToneProbability = 0.2f;
    config.velocity = 0.75f;
    config.gateFraction = 0.8f;
    return config;
}

} // namespace

TEST_CASE("GenerateBassline: slot 0 of every span is always the chord root") {
    GeneratedBasslineConfig config = MakeConfig();
    config.passingToneProbability = 1.0f; // maximize the chance later slots diverge
    RandomSource rng(123);
    std::vector<StepConfig> steps = GenerateBassline(config, rng);

    int slotsPerSpan = static_cast<int>(config.chordLengthBeats / config.noteLengthBeats);
    REQUIRE(steps.size() == config.degrees.size() * static_cast<size_t>(slotsPerSpan));

    for (size_t i = 0; i < config.degrees.size(); ++i) {
        const StepConfig& rootStep = steps[i * static_cast<size_t>(slotsPerSpan)];
        NoteName expectedNote;
        int expectedOctave;
        Theory::DegreeToNote(config.key, config.scale, config.degrees[i], config.baseOctave, expectedNote,
                              expectedOctave);
        CHECK(rootStep.note == expectedNote);
        CHECK(rootStep.octave == expectedOctave);
    }
}

TEST_CASE("GenerateBassline: passingToneProbability 0 makes the whole line root-only") {
    GeneratedBasslineConfig config = MakeConfig();
    config.passingToneProbability = 0.0f;
    RandomSource rng(7);
    std::vector<StepConfig> steps = GenerateBassline(config, rng);

    int slotsPerSpan = static_cast<int>(config.chordLengthBeats / config.noteLengthBeats);
    for (size_t i = 0; i < config.degrees.size(); ++i) {
        NoteName expectedNote;
        int expectedOctave;
        Theory::DegreeToNote(config.key, config.scale, config.degrees[i], config.baseOctave, expectedNote,
                              expectedOctave);
        for (int slot = 0; slot < slotsPerSpan; ++slot) {
            const StepConfig& step = steps[i * static_cast<size_t>(slotsPerSpan) + static_cast<size_t>(slot)];
            CHECK(step.note == expectedNote);
            CHECK(step.octave == expectedOctave);
        }
    }
}

TEST_CASE("GenerateBassline: same seed produces identical output") {
    GeneratedBasslineConfig config = MakeConfig();
    RandomSource rngA(42);
    RandomSource rngB(42);

    std::vector<StepConfig> a = GenerateBassline(config, rngA);
    std::vector<StepConfig> b = GenerateBassline(config, rngB);

    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].note == b[i].note);
        CHECK(a[i].octave == b[i].octave);
        CHECK(a[i].beat == doctest::Approx(b[i].beat));
    }
}

TEST_CASE("GenerateBassline: beats stay within [0, degrees.size() * chordLengthBeats)") {
    GeneratedBasslineConfig config = MakeConfig();
    RandomSource rng(9);
    std::vector<StepConfig> steps = GenerateBassline(config, rng);

    double totalLength = static_cast<double>(config.degrees.size()) * config.chordLengthBeats;
    for (const StepConfig& step : steps) {
        CHECK(step.beat >= 0.0);
        CHECK(step.beat < totalLength);
        CHECK(step.instrument == "bass");
        CHECK(step.hasNote);
    }
}

TEST_CASE("GenerateBassline throws on empty degrees or non-positive lengths") {
    RandomSource rng(1);

    GeneratedBasslineConfig emptyDegrees = MakeConfig();
    emptyDegrees.degrees.clear();
    CHECK_THROWS_AS(GenerateBassline(emptyDegrees, rng), std::runtime_error);

    GeneratedBasslineConfig badChordLength = MakeConfig();
    badChordLength.chordLengthBeats = 0.0;
    CHECK_THROWS_AS(GenerateBassline(badChordLength, rng), std::runtime_error);

    GeneratedBasslineConfig badNoteLength = MakeConfig();
    badNoteLength.noteLengthBeats = -1.0;
    CHECK_THROWS_AS(GenerateBassline(badNoteLength, rng), std::runtime_error);
}
