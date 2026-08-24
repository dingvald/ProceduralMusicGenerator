#include "doctest/doctest.h"
#include "engine/Envelope.h"

using namespace pmg;

TEST_CASE("Envelope ADSR stage timing and amplitude") {
    Envelope env;
    ADSRParams params;
    params.attackSec = 0.01f;   // 10 samples @ 1000Hz
    params.decaySec = 0.02f;    // 20 samples
    params.sustainLevel = 0.5f;
    params.releaseSec = 0.01f;  // 10 samples from sustain level

    const uint32_t sampleRate = 1000;
    env.Configure(params, sampleRate);

    CHECK(env.IsFinished()); // idle before NoteOn

    env.NoteOn();
    CHECK_FALSE(env.IsFinished());

    float last = 0.0f;
    for (int i = 0; i < 10; ++i) {
        last = env.NextSample();
    }
    CHECK(last == doctest::Approx(1.0f).epsilon(0.05));
    CHECK(env.GetStage() == Envelope::Stage::Decay);

    // A couple of extra samples beyond the nominal 20-sample decay window
    // give headroom for float rounding in the per-sample decay rate to
    // cross the sustain threshold.
    for (int i = 0; i < 25; ++i) {
        last = env.NextSample();
    }
    CHECK(last == doctest::Approx(params.sustainLevel).epsilon(0.05));
    CHECK(env.GetStage() == Envelope::Stage::Sustain);

    for (int i = 0; i < 5; ++i) {
        last = env.NextSample();
    }
    CHECK(last == doctest::Approx(params.sustainLevel).epsilon(0.001));

    env.NoteOff();
    CHECK(env.GetStage() == Envelope::Stage::Release);
    for (int i = 0; i < 20; ++i) {
        last = env.NextSample();
    }
    CHECK(last == doctest::Approx(0.0f).epsilon(0.05));
    CHECK(env.IsFinished());
}
