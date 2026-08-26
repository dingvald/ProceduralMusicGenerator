#include <cmath>

#include "doctest/doctest.h"
#include "engine/DelayProcessor.h"

using namespace pmg;

TEST_CASE("DelayProcessor with default config (mix == 0) passes samples through unchanged") {
    DelayProcessor processor;
    processor.Configure(DelayConfig{}, 48000); // delayTimeSeconds/feedback/mix all 0 -> disabled

    float inputs[] = {-1.0f, -0.5f, 0.0f, 0.25f, 0.777f, 1.0f};
    for (float input : inputs) {
        CHECK(processor.Process(input) == doctest::Approx(input));
    }
}

TEST_CASE("DelayProcessor echoes a unit impulse exactly delaySamples later") {
    const uint32_t sampleRate = 1000;
    DelayConfig config;
    config.delayTimeSeconds = 0.01f; // -> 10 samples @ 1000Hz
    config.feedback = 0.0f;
    config.mix = 0.5f;

    DelayProcessor processor;
    processor.Configure(config, sampleRate);

    CHECK(processor.Process(1.0f) == doctest::Approx(0.5f)); // call 0: dry*(1-mix), delay buffer was silent

    for (int i = 1; i < 10; ++i) {
        CHECK(processor.Process(0.0f) == doctest::Approx(0.0f));
    }

    // Call 10 (10 samples after the impulse): the echo tap now reads back
    // the impulse, so a silent input still produces mix-scaled output.
    CHECK(processor.Process(0.0f) == doctest::Approx(0.5f));

    for (int i = 11; i < 20; ++i) {
        CHECK(processor.Process(0.0f) == doctest::Approx(0.0f));
    }
}

TEST_CASE("DelayProcessor feedback produces a second, decayed echo one delay period later") {
    const uint32_t sampleRate = 1000;
    DelayConfig config;
    config.delayTimeSeconds = 0.01f; // -> 10 samples @ 1000Hz
    config.feedback = 0.4f;
    config.mix = 0.5f;

    DelayProcessor processor;
    processor.Configure(config, sampleRate);

    processor.Process(1.0f); // impulse at call 0

    float firstEcho = 0.0f;
    for (int i = 1; i < 20; ++i) {
        float sample = processor.Process(0.0f);
        if (i == 10) firstEcho = sample;
    }
    float secondEcho = processor.Process(0.0f); // call 20

    CHECK(firstEcho == doctest::Approx(0.5f));         // 1.0 (impulse) * mix
    CHECK(secondEcho == doctest::Approx(0.4f * 0.5f)); // 1.0 * feedback * mix
}

TEST_CASE("DelayProcessor clamps feedback below 1.0 so output never grows unbounded") {
    DelayConfig config;
    config.delayTimeSeconds = 0.002f;
    config.feedback = 5.0f; // way over 1.0 -- must be clamped, or this would diverge
    config.mix = 0.9f;

    DelayProcessor processor;
    processor.Configure(config, 48000);

    processor.Process(1.0f);
    for (int i = 0; i < 5000; ++i) {
        float sample = processor.Process(0.0f);
        CHECK(std::fabs(sample) <= 1.0001f);
    }
}

TEST_CASE("DelayProcessor clamps an excessive delayTimeSeconds instead of crashing") {
    DelayConfig config;
    config.delayTimeSeconds = 1000.0f; // would need far more than the max buffer at this rate
    config.feedback = 0.0f;
    config.mix = 0.5f;

    const uint32_t sampleRate = 1000;
    DelayProcessor processor;
    processor.Configure(config, sampleRate);

    processor.Process(1.0f); // impulse

    bool echoHeard = false;
    for (size_t i = 0; i < DelayProcessor::kMaxDelayBufferSamples; ++i) {
        if (processor.Process(0.0f) > 0.01f) {
            echoHeard = true;
            break;
        }
    }
    CHECK(echoHeard); // the delay was clamped to the buffer's max, not left unbounded/silent forever
}
