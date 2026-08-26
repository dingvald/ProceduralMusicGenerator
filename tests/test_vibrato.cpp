#include <cmath>

#include "doctest/doctest.h"
#include "engine/Vibrato.h"

using namespace pmg;

TEST_CASE("Vibrato with depthCents == 0 always returns multiplier 1.0") {
    Vibrato vibrato;
    VibratoConfig config; // depthCents left at default 0 -> disabled
    vibrato.Configure(config, 48000);
    vibrato.NoteOn();

    for (int i = 0; i < 200; ++i) {
        CHECK(vibrato.NextMultiplier() == doctest::Approx(1.0f));
    }
}

TEST_CASE("Vibrato traces a sine-shaped pitch multiplier over its cycle") {
    const uint32_t sampleRate = 1000;
    VibratoConfig config;
    config.rateHz = 125.0f;      // -> 8 samples/cycle @ 1000Hz
    config.depthCents = 1200.0f; // one octave, so peak/trough are exactly 2.0/0.5

    Vibrato vibrato;
    vibrato.Configure(config, sampleRate);
    vibrato.NoteOn();

    // Call k (1-indexed) reads phase = (k-1)/8. Sample the four cardinal
    // points of the cycle: phase 0 (center), 0.25 (peak), 0.5 (center, on
    // the way down), 0.75 (trough).
    CHECK(vibrato.NextMultiplier() == doctest::Approx(1.0f));  // call 1: phase 0
    vibrato.NextMultiplier();                                  // call 2: phase 1/8
    CHECK(vibrato.NextMultiplier() == doctest::Approx(2.0f));  // call 3: phase 2/8 (peak)
    vibrato.NextMultiplier();                                  // call 4: phase 3/8
    CHECK(vibrato.NextMultiplier() == doctest::Approx(1.0f));  // call 5: phase 4/8
    vibrato.NextMultiplier();                                  // call 6: phase 5/8
    CHECK(vibrato.NextMultiplier() == doctest::Approx(0.5f));  // call 7: phase 6/8 (trough)
    vibrato.NextMultiplier();                                  // call 8: phase 7/8
    CHECK(vibrato.NextMultiplier() == doctest::Approx(1.0f));  // call 9: phase wraps back to 0
}

TEST_CASE("Vibrato NoteOn() resets the LFO phase to the start of its cycle") {
    const uint32_t sampleRate = 1000;
    VibratoConfig config;
    config.rateHz = 125.0f;
    config.depthCents = 1200.0f;

    Vibrato vibrato;
    vibrato.Configure(config, sampleRate);
    vibrato.NoteOn();

    vibrato.NextMultiplier(); // phase 0
    vibrato.NextMultiplier(); // phase 1/8
    float midCycle = vibrato.NextMultiplier(); // phase 2/8 (peak)
    CHECK(midCycle == doctest::Approx(2.0f));

    vibrato.NoteOn(); // simulate a new note being triggered
    float afterReset = vibrato.NextMultiplier();
    CHECK(afterReset == doctest::Approx(1.0f)); // back to phase 0, not continuing mid-cycle
}
