#include <cmath>
#include <stdexcept>

#include "doctest/doctest.h"
#include "engine/MelodyGenerator.h"
#include "engine/RandomSource.h"

using namespace pmg;

namespace {

GeneratedMelodyConfig MakeConfig() {
    GeneratedMelodyConfig config;
    config.instrument = "lead";
    config.key = NoteName::A;
    config.scale = Scale::MinorPentatonic;
    config.baseOctave = 4;
    config.octaveRange = 1;
    config.lengthBeats = 8.0;
    config.noteLengthBeats = 1.0;
    config.restProbability = 0.0f; // deterministic note count for most tests
    config.gateFraction = 0.8f;
    config.velocity = 0.8f;
    return config;
}

bool NoteInScale(NoteName key, Scale scale, NoteName note) {
    const std::vector<int>& intervals = Theory::ScaleIntervals(scale);
    int relative = ((static_cast<int>(note) - static_cast<int>(key)) % 12 + 12) % 12;
    for (int interval : intervals) {
        if (interval == relative) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("GenerateMelody: same seed produces identical output") {
    GeneratedMelodyConfig config = MakeConfig();
    RandomSource rngA(1234);
    RandomSource rngB(1234);

    std::vector<StepConfig> a = GenerateMelody(config, rngA);
    std::vector<StepConfig> b = GenerateMelody(config, rngB);

    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].note == b[i].note);
        CHECK(a[i].octave == b[i].octave);
        CHECK(a[i].beat == doctest::Approx(b[i].beat));
    }
}

TEST_CASE("GenerateMelody: different seeds usually produce different output") {
    GeneratedMelodyConfig config = MakeConfig();
    config.octaveRange = 2; // give the walk more room to diverge
    RandomSource rngA(1);
    RandomSource rngB(2);

    std::vector<StepConfig> a = GenerateMelody(config, rngA);
    std::vector<StepConfig> b = GenerateMelody(config, rngB);

    bool anyDifferent = false;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        if (a[i].note != b[i].note || a[i].octave != b[i].octave) {
            anyDifferent = true;
        }
    }
    CHECK(anyDifferent);
}

TEST_CASE("GenerateMelody: every emitted note lies in the configured scale") {
    GeneratedMelodyConfig config = MakeConfig();
    RandomSource rng(42);
    std::vector<StepConfig> steps = GenerateMelody(config, rng);

    REQUIRE_FALSE(steps.empty());
    for (const StepConfig& step : steps) {
        CHECK(step.hasNote);
        CHECK(NoteInScale(config.key, config.scale, step.note));
    }
}

TEST_CASE("GenerateMelody: octave never leaves [baseOctave, baseOctave + octaveRange]") {
    GeneratedMelodyConfig config = MakeConfig();
    config.baseOctave = 3;
    config.octaveRange = 2;
    RandomSource rng(7);
    std::vector<StepConfig> steps = GenerateMelody(config, rng);

    for (const StepConfig& step : steps) {
        CHECK(step.octave >= config.baseOctave);
        CHECK(step.octave <= config.baseOctave + config.octaveRange);
    }
}

TEST_CASE("GenerateMelody: octaveRange 0 pins every note to baseOctave") {
    GeneratedMelodyConfig config = MakeConfig();
    config.octaveRange = 0;
    RandomSource rng(99);
    std::vector<StepConfig> steps = GenerateMelody(config, rng);

    REQUIRE_FALSE(steps.empty());
    for (const StepConfig& step : steps) {
        CHECK(step.octave == config.baseOctave);
    }
}

TEST_CASE("GenerateMelody: restProbability 0 never rests -- one note per slot") {
    GeneratedMelodyConfig config = MakeConfig();
    config.restProbability = 0.0f;
    RandomSource rng(5);
    std::vector<StepConfig> steps = GenerateMelody(config, rng);

    int expectedSlots = static_cast<int>(config.lengthBeats / config.noteLengthBeats);
    CHECK(steps.size() == static_cast<size_t>(expectedSlots));
}

TEST_CASE("GenerateMelody: restProbability 1 always rests -- no notes at all") {
    GeneratedMelodyConfig config = MakeConfig();
    config.restProbability = 1.0f;
    RandomSource rng(5);
    std::vector<StepConfig> steps = GenerateMelody(config, rng);

    CHECK(steps.empty());
}

TEST_CASE("GenerateMelody: notes land on the expected beat grid and stay instrument-tagged") {
    GeneratedMelodyConfig config = MakeConfig();
    RandomSource rng(11);
    std::vector<StepConfig> steps = GenerateMelody(config, rng);

    for (const StepConfig& step : steps) {
        CHECK(step.instrument == "lead");
        CHECK(step.gate == doctest::Approx(config.noteLengthBeats * config.gateFraction));
        double remainder = std::fmod(step.beat, config.noteLengthBeats);
        CHECK((remainder < 1e-9 || config.noteLengthBeats - remainder < 1e-9));
    }
}

TEST_CASE("GenerateMelody throws on non-positive lengthBeats/noteLengthBeats or negative octaveRange") {
    RandomSource rng(1);

    GeneratedMelodyConfig badLength = MakeConfig();
    badLength.lengthBeats = 0.0;
    CHECK_THROWS_AS(GenerateMelody(badLength, rng), std::runtime_error);

    GeneratedMelodyConfig badNoteLength = MakeConfig();
    badNoteLength.noteLengthBeats = -1.0;
    CHECK_THROWS_AS(GenerateMelody(badNoteLength, rng), std::runtime_error);

    GeneratedMelodyConfig badRange = MakeConfig();
    badRange.octaveRange = -1;
    CHECK_THROWS_AS(GenerateMelody(badRange, rng), std::runtime_error);
}
