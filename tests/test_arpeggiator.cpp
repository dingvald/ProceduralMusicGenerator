#include <cmath>

#include "doctest/doctest.h"
#include "engine/Arpeggiator.h"

using namespace pmg;

TEST_CASE("Arpeggiator with no configured offsets always returns multiplier 1.0") {
    Arpeggiator arp;
    ArpeggioConfig config; // semitoneOffsets left empty -> disabled
    arp.Configure(config, 48000);
    arp.NoteOn();

    for (int i = 0; i < 200; ++i) {
        CHECK(arp.NextMultiplier() == doctest::Approx(1.0f));
    }
}

TEST_CASE("Arpeggiator cycles through semitone offsets at the configured step rate") {
    const uint32_t sampleRate = 1000;
    ArpeggioConfig config;
    config.semitoneOffsets = {0, 12}; // root, then one octave up
    config.rateHz = 100.0f;           // -> 10 samples/step @ 1000Hz

    Arpeggiator arp;
    arp.Configure(config, sampleRate);
    arp.NoteOn();

    for (int i = 0; i < 10; ++i) {
        CHECK(arp.NextMultiplier() == doctest::Approx(1.0f)); // 2^(0/12)
    }
    for (int i = 0; i < 10; ++i) {
        CHECK(arp.NextMultiplier() == doctest::Approx(2.0f)); // 2^(12/12)
    }
    for (int i = 0; i < 10; ++i) {
        CHECK(arp.NextMultiplier() == doctest::Approx(1.0f)); // wrapped back to the first offset
    }
}

TEST_CASE("Arpeggiator NoteOn() restarts the pattern from its first offset") {
    const uint32_t sampleRate = 1000;
    ArpeggioConfig config;
    config.semitoneOffsets = {0, 7, 12};
    config.rateHz = 1000.0f; // -> 1 sample/step, so the pattern advances every call

    Arpeggiator arp;
    arp.Configure(config, sampleRate);
    arp.NoteOn();

    arp.NextMultiplier(); // step 0 (offset 0)
    arp.NextMultiplier(); // step 1 (offset 7)
    float midPattern = arp.NextMultiplier();
    CHECK(midPattern == doctest::Approx(std::pow(2.0f, 12.0f / 12.0f))); // step 2 (offset 12)

    arp.NoteOn(); // simulate a new note being triggered
    float afterReset = arp.NextMultiplier();
    CHECK(afterReset == doctest::Approx(1.0f)); // back to offset 0, not continuing from step 3
}
