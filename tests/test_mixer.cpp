#include <cmath>

#include "doctest/doctest.h"
#include "engine/Mixer.h"

using namespace pmg;

TEST_CASE("Mixer plays a synth note and settles to silence after release") {
    Mixer mixer;
    const uint32_t sampleRate = 1000;
    mixer.Configure(sampleRate);

    SynthInstrumentDef def;
    def.waveform = Waveform::Sine;
    def.envelope.attackSec = 0.005f;
    def.envelope.decaySec = 0.005f;
    def.envelope.sustainLevel = 0.8f;
    def.envelope.releaseSec = 0.005f;
    def.gain = 1.0f;
    mixer.AddSynthInstrument("test_synth", def);

    int handle = mixer.NoteOn("test_synth", 100.0f, 1.0f);
    CHECK(handle >= 0);

    bool heardSound = false;
    for (int i = 0; i < 20; ++i) {
        if (std::fabs(mixer.RenderNextSample()) > 0.01f) {
            heardSound = true;
        }
    }
    CHECK(heardSound);

    mixer.NoteOff(handle);
    for (int i = 0; i < 20; ++i) {
        mixer.RenderNextSample();
    }

    float silence = mixer.RenderNextSample();
    CHECK(std::fabs(silence) < 0.05f);

    // The voice slot should be reusable after the envelope finishes.
    int handle2 = mixer.NoteOn("test_synth", 100.0f, 1.0f);
    CHECK(handle2 >= 0);
}

TEST_CASE("Mixer drops NoteOn for an unknown instrument") {
    Mixer mixer;
    mixer.Configure(1000);
    CHECK(mixer.NoteOn("does_not_exist", 440.0f, 1.0f) == -1);
}

TEST_CASE("Mixer suppresses NoteOn on a muted track") {
    Mixer mixer;
    mixer.Configure(1000);

    SynthInstrumentDef def;
    def.waveform = Waveform::Square;
    mixer.AddSynthInstrument("lead", def);

    mixer.SetTrackMuted("lead", true);
    CHECK(mixer.NoteOn("lead", 100.0f, 1.0f) == -1);

    mixer.SetTrackMuted("lead", false);
    CHECK(mixer.NoteOn("lead", 100.0f, 1.0f) >= 0);
}
