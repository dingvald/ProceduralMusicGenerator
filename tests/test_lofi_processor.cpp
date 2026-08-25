#include <cmath>
#include <set>

#include "doctest/doctest.h"
#include "engine/LoFiProcessor.h"

using namespace pmg;

TEST_CASE("LoFiProcessor with default config passes samples through unchanged") {
    LoFiProcessor processor;
    processor.Configure(LoFiConfig{}); // bitDepth = 16, holdFactor = 1

    float inputs[] = {-1.0f, -0.5f, -0.123f, 0.0f, 0.25f, 0.777f, 1.0f};
    for (float input : inputs) {
        CHECK(processor.Process(input) == doctest::Approx(input));
    }
}

TEST_CASE("LoFiProcessor bitDepth quantizes output to 2^bitDepth distinct levels") {
    LoFiConfig config;
    config.bitDepth = 3; // 8 levels
    config.holdFactor = 1;

    LoFiProcessor processor;
    processor.Configure(config);

    std::set<float> distinctOutputs;
    const int steps = 4000;
    for (int i = 0; i <= steps; ++i) {
        float input = -1.0f + 2.0f * static_cast<float>(i) / steps; // ramp across [-1, 1]
        distinctOutputs.insert(processor.Process(input));
    }

    CHECK(distinctOutputs.size() <= 8);
    CHECK(distinctOutputs.size() >= 6); // a dense ramp should hit nearly all levels
}

TEST_CASE("LoFiProcessor holdFactor holds the output for N samples before updating") {
    LoFiConfig config;
    config.bitDepth = 16; // isolate hold behavior from quantization
    config.holdFactor = 4;

    LoFiProcessor processor;
    processor.Configure(config);

    float first = processor.Process(0.1f);
    CHECK(processor.Process(0.2f) == doctest::Approx(first));
    CHECK(processor.Process(0.3f) == doctest::Approx(first));
    CHECK(processor.Process(0.4f) == doctest::Approx(first));

    float second = processor.Process(0.5f); // 5th call: hold window rolls over
    CHECK(second == doctest::Approx(0.5f));
    CHECK(processor.Process(0.6f) == doctest::Approx(second));
}

TEST_CASE("LoFiProcessor clamps out-of-range config instead of misbehaving") {
    LoFiConfig config;
    config.bitDepth = 0;    // below valid range
    config.holdFactor = -3; // invalid; would be UB as a modulus if left uncorrected

    LoFiProcessor processor;
    processor.Configure(config);

    for (int i = 0; i < 100; ++i) {
        float sample = processor.Process(0.37f);
        CHECK(sample >= -1.0001f);
        CHECK(sample <= 1.0001f);
    }
}

TEST_CASE("LoFiProcessor clamps input outside [-1, 1] before quantizing") {
    LoFiConfig config;
    config.bitDepth = 4;
    config.holdFactor = 1;

    LoFiProcessor processor;
    processor.Configure(config);

    CHECK(processor.Process(5.0f) == doctest::Approx(1.0f));
    CHECK(processor.Process(-5.0f) == doctest::Approx(-1.0f));
}
