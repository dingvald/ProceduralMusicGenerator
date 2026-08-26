#include "doctest/doctest.h"
#include "engine/GainCrossfader.h"

using namespace pmg;

namespace {

GainCrossfadeConfig MakeConfig() {
    GainCrossfadeConfig config;
    config.instrument = "pad";
    config.parameter = "danger";
    config.paramAtGainMin = 0.0f;
    config.gainAtMin = 0.0f;
    config.paramAtGainMax = 1.0f;
    config.gainAtMax = 1.0f;
    config.smoothingSeconds = 1.0f;
    return config;
}

} // namespace

TEST_CASE("GainCrossfader jumps straight to the target on its first tick") {
    GainCrossfader crossfader;
    crossfader.Configure(MakeConfig());

    // deltaSeconds is irrelevant on the first tick -- no ramp-from-silence.
    float gain = crossfader.NextGain(0.5f, 0.0);
    CHECK(gain == doctest::Approx(0.5f));
    CHECK(crossfader.CurrentGain() == doctest::Approx(0.5f));
}

TEST_CASE("GainCrossfader ramps toward the target at the configured rate") {
    GainCrossfader crossfader;
    crossfader.Configure(MakeConfig());
    crossfader.NextGain(0.0f, 0.0); // establish current gain at 0.0

    // Full range (0..1) over smoothingSeconds == 1.0s -> rate of 1.0/s.
    // Half a second toward a target of 1.0 should cross about half the range.
    float gain = crossfader.NextGain(1.0f, 0.5);
    CHECK(gain == doctest::Approx(0.5f).epsilon(0.01));
}

TEST_CASE("GainCrossfader clamps to the target rather than overshooting") {
    GainCrossfader crossfader;
    crossfader.Configure(MakeConfig());
    crossfader.NextGain(0.0f, 0.0);

    // A huge deltaSeconds would overshoot a naive fixed-step ramp; the
    // result must land exactly on the target instead.
    float gain = crossfader.NextGain(1.0f, 100.0);
    CHECK(gain == doctest::Approx(1.0f));
}

TEST_CASE("GainCrossfader clamps parameterValue outside [paramAtGainMin, paramAtGainMax]") {
    GainCrossfader crossfader;
    crossfader.Configure(MakeConfig());

    CHECK(crossfader.NextGain(-5.0f, 0.0) == doctest::Approx(0.0f));

    GainCrossfader crossfader2;
    crossfader2.Configure(MakeConfig());
    CHECK(crossfader2.NextGain(5.0f, 0.0) == doctest::Approx(1.0f));
}

TEST_CASE("GainCrossfader smoothingSeconds <= 0 snaps instantly on every tick") {
    GainCrossfadeConfig config = MakeConfig();
    config.smoothingSeconds = 0.0f;
    GainCrossfader crossfader;
    crossfader.Configure(config);

    crossfader.NextGain(0.0f, 0.0);
    float gain = crossfader.NextGain(1.0f, 0.001); // tiny deltaSeconds, still snaps fully
    CHECK(gain == doctest::Approx(1.0f));
}

TEST_CASE("GainCrossfader supports a non-[0,1] parameter/gain mapping") {
    GainCrossfadeConfig config;
    config.instrument = "bass";
    config.parameter = "intensity";
    config.paramAtGainMin = 10.0f;
    config.gainAtMin = 0.2f;
    config.paramAtGainMax = 20.0f;
    config.gainAtMax = 0.8f;
    config.smoothingSeconds = 0.001f; // effectively instant for this test

    GainCrossfader crossfader;
    crossfader.Configure(config);
    crossfader.NextGain(10.0f, 0.0); // first tick: jumps straight to gainAtMin regardless

    float midGain = crossfader.NextGain(15.0f, 10.0); // midpoint of the param range, ample time to settle
    CHECK(midGain == doctest::Approx(0.5f).epsilon(0.01));
}
