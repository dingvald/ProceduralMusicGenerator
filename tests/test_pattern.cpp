#include "doctest/doctest.h"
#include "engine/Pattern.h"

using namespace pmg;

TEST_CASE("ResolvePattern resolves a note-based step via Theory::NoteToFrequency") {
    PatternConfig config;
    config.id = "p1";
    config.lengthBars = 1;

    StepConfig step;
    step.beat = 0.5;
    step.instrument = "lead";
    step.hasNote = true;
    step.note = NoteName::A;
    step.octave = 4;
    step.velocity = 0.7f;
    step.gate = 0.3f;
    config.steps.push_back(step);

    Pattern pattern = ResolvePattern(config);

    REQUIRE(pattern.steps.size() == 1);
    CHECK(pattern.steps[0].isSynth);
    CHECK(pattern.steps[0].frequencyHz == doctest::Approx(Theory::NoteToFrequency(NoteName::A, 4)));
    CHECK(pattern.steps[0].beatOffset == doctest::Approx(0.5));
    CHECK(pattern.steps[0].instrument == "lead");
    CHECK(pattern.steps[0].velocity == doctest::Approx(0.7f));
    CHECK(pattern.steps[0].gate == doctest::Approx(0.3f));
}

TEST_CASE("ResolvePattern marks a plain (no note) step as a sample trigger") {
    PatternConfig config;
    config.id = "p1";

    StepConfig step;
    step.beat = 0.0;
    step.instrument = "kick";
    config.steps.push_back(step);

    Pattern pattern = ResolvePattern(config);

    REQUIRE(pattern.steps.size() == 1);
    CHECK_FALSE(pattern.steps[0].isSynth);
    CHECK(pattern.steps[0].frequencyHz == doctest::Approx(0.0f));
}

TEST_CASE("ResolvePattern resolves a mixed pattern of note and sample-trigger steps") {
    PatternConfig config;
    config.id = "p1";

    StepConfig noteStep;
    noteStep.beat = 1.0;
    noteStep.instrument = "lead";
    noteStep.hasNote = true;
    noteStep.note = NoteName::C;
    noteStep.octave = 5;
    config.steps.push_back(noteStep);

    StepConfig sampleStep;
    sampleStep.beat = 0.0;
    sampleStep.instrument = "kick";
    config.steps.push_back(sampleStep);

    Pattern pattern = ResolvePattern(config);

    REQUIRE(pattern.steps.size() == 2);
    // Sorted ascending by beat, so the kick (beat 0.0) comes first even
    // though it was pushed second.
    CHECK_FALSE(pattern.steps[0].isSynth);
    CHECK(pattern.steps[0].instrument == "kick");
    CHECK(pattern.steps[1].isSynth);
    CHECK(pattern.steps[1].instrument == "lead");
    CHECK(pattern.steps[1].frequencyHz == doctest::Approx(Theory::NoteToFrequency(NoteName::C, 5)));
}

TEST_CASE("ResolvePattern sorts steps ascending by beat and preserves order on ties") {
    PatternConfig config;
    config.id = "p1";

    StepConfig late;
    late.beat = 2.0;
    late.instrument = "a";
    config.steps.push_back(late);

    StepConfig earlyFirst;
    earlyFirst.beat = 0.0;
    earlyFirst.instrument = "b";
    config.steps.push_back(earlyFirst);

    StepConfig earlySecond; // same beat as earlyFirst; stable_sort must keep it after
    earlySecond.beat = 0.0;
    earlySecond.instrument = "c";
    config.steps.push_back(earlySecond);

    Pattern pattern = ResolvePattern(config);

    REQUIRE(pattern.steps.size() == 3);
    CHECK(pattern.steps[0].instrument == "b");
    CHECK(pattern.steps[1].instrument == "c");
    CHECK(pattern.steps[2].instrument == "a");
}

TEST_CASE("ResolvePattern copies id and lengthBars through unchanged") {
    PatternConfig config;
    config.id = "verse";
    config.lengthBars = 4;

    Pattern pattern = ResolvePattern(config);

    CHECK(pattern.id == "verse");
    CHECK(pattern.lengthBars == 4);
    CHECK(pattern.steps.empty());
}
