#include <stdexcept>
#include <string>

#include "doctest/doctest.h"
#include "engine/RandomSource.h"
#include "engine/RhythmGenerator.h"

using namespace pmg;

namespace {

GeneratedRhythmConfig MakeConfig(int steps, int pulses) {
    GeneratedRhythmConfig config;
    config.instrument = "hihat";
    config.steps = steps;
    config.pulses = pulses;
    config.lengthBeats = 4.0;
    return config;
}

// Renders a generated rhythm back into a "10010010"-style bit string for
// easy comparison against known Euclidean-rhythm reference patterns.
std::string ToBitString(const std::vector<StepConfig>& hits, int steps, double lengthBeats) {
    std::string bits(static_cast<size_t>(steps), '0');
    double stepBeats = lengthBeats / steps;
    for (const StepConfig& hit : hits) {
        int index = static_cast<int>(hit.beat / stepBeats + 0.5);
        bits[static_cast<size_t>(index)] = '1';
    }
    return bits;
}

} // namespace

TEST_CASE("GenerateRhythm: E(3,8) matches the canonical Euclidean rhythm 10010010") {
    GeneratedRhythmConfig config = MakeConfig(8, 3);
    RandomSource rng(1);
    std::vector<StepConfig> hits = GenerateRhythm(config, rng);
    CHECK(ToBitString(hits, 8, config.lengthBeats) == "10010010");
}

TEST_CASE("GenerateRhythm: E(5,8) matches the canonical Euclidean rhythm 10110110 (Cuban cinquillo)") {
    GeneratedRhythmConfig config = MakeConfig(8, 5);
    RandomSource rng(1);
    std::vector<StepConfig> hits = GenerateRhythm(config, rng);
    CHECK(ToBitString(hits, 8, config.lengthBeats) == "10110110");
}

TEST_CASE("GenerateRhythm: pulse count in the output matches config.pulses") {
    RandomSource rng(1);
    for (int pulses = 0; pulses <= 8; ++pulses) {
        GeneratedRhythmConfig config = MakeConfig(8, pulses);
        std::vector<StepConfig> hits = GenerateRhythm(config, rng);
        CHECK(hits.size() == static_cast<size_t>(pulses));
    }
}

TEST_CASE("GenerateRhythm: pulses <= 0 produces no hits") {
    GeneratedRhythmConfig config = MakeConfig(16, 0);
    RandomSource rng(1);
    CHECK(GenerateRhythm(config, rng).empty());

    config.pulses = -3;
    CHECK(GenerateRhythm(config, rng).empty());
}

TEST_CASE("GenerateRhythm: pulses >= steps fills every slot") {
    GeneratedRhythmConfig config = MakeConfig(8, 10);
    RandomSource rng(1);
    std::vector<StepConfig> hits = GenerateRhythm(config, rng);
    CHECK(hits.size() == 8);
}

TEST_CASE("GenerateRhythm: hit beats stay within [0, lengthBeats)") {
    GeneratedRhythmConfig config = MakeConfig(16, 11);
    RandomSource rng(3);
    std::vector<StepConfig> hits = GenerateRhythm(config, rng);

    REQUIRE_FALSE(hits.empty());
    for (const StepConfig& hit : hits) {
        CHECK(hit.beat >= 0.0);
        CHECK(hit.beat < config.lengthBeats);
        CHECK(hit.instrument == "hihat");
    }
}

TEST_CASE("GenerateRhythm: hasNote/note/octave pass through to every hit when set") {
    GeneratedRhythmConfig config = MakeConfig(8, 3);
    config.hasNote = true;
    config.note = NoteName::C;
    config.octave = 8;
    RandomSource rng(1);
    std::vector<StepConfig> hits = GenerateRhythm(config, rng);

    REQUIRE_FALSE(hits.empty());
    for (const StepConfig& hit : hits) {
        CHECK(hit.hasNote);
        CHECK(hit.note == NoteName::C);
        CHECK(hit.octave == 8);
    }
}

TEST_CASE("GenerateRhythm: zero jitter is fully deterministic regardless of rng state") {
    GeneratedRhythmConfig config = MakeConfig(8, 5);
    config.velocityJitter = 0.0f;

    RandomSource rngA(1);
    RandomSource rngB(999);
    std::vector<StepConfig> a = GenerateRhythm(config, rngA);
    std::vector<StepConfig> b = GenerateRhythm(config, rngB);

    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].velocity == doctest::Approx(b[i].velocity));
        CHECK(a[i].velocity == doctest::Approx(config.velocity));
    }
}

TEST_CASE("GenerateRhythm: positive jitter keeps velocity within [0,1] and same seed is reproducible") {
    GeneratedRhythmConfig config = MakeConfig(8, 5);
    config.velocity = 0.9f;
    config.velocityJitter = 0.5f;

    RandomSource rngA(42);
    RandomSource rngB(42);
    std::vector<StepConfig> a = GenerateRhythm(config, rngA);
    std::vector<StepConfig> b = GenerateRhythm(config, rngB);

    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].velocity >= 0.0f);
        CHECK(a[i].velocity <= 1.0f);
        CHECK(a[i].velocity == doctest::Approx(b[i].velocity));
    }
}

TEST_CASE("GenerateRhythm throws on non-positive steps/lengthBeats or negative velocityJitter") {
    RandomSource rng(1);

    GeneratedRhythmConfig badSteps = MakeConfig(0, 0);
    CHECK_THROWS_AS(GenerateRhythm(badSteps, rng), std::runtime_error);

    GeneratedRhythmConfig badLength = MakeConfig(8, 3);
    badLength.lengthBeats = 0.0;
    CHECK_THROWS_AS(GenerateRhythm(badLength, rng), std::runtime_error);

    GeneratedRhythmConfig badJitter = MakeConfig(8, 3);
    badJitter.velocityJitter = -0.1f;
    CHECK_THROWS_AS(GenerateRhythm(badJitter, rng), std::runtime_error);
}
