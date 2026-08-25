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

TEST_CASE("Mixer NoteOn with a gate duration auto-releases and frees its voice without an explicit NoteOff") {
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

    const int gateDurationSamples = 15;
    int handle = mixer.NoteOn("test_synth", 100.0f, 1.0f, gateDurationSamples);
    CHECK(handle >= 0);

    // No NoteOff() call anywhere in this test: render well past gate +
    // release and confirm the voice freed itself and is reusable.
    for (int i = 0; i < 60; ++i) {
        mixer.RenderNextSample();
    }

    int handle2 = mixer.NoteOn("test_synth", 100.0f, 1.0f);
    CHECK(handle2 >= 0);
}

TEST_CASE("Mixer drops NoteOn for an unknown instrument") {
    Mixer mixer;
    mixer.Configure(1000);
    CHECK(mixer.NoteOn("does_not_exist", 440.0f, 1.0f) == -1);
}

TEST_CASE("Mixer reuses voices for a one-shot (zero-sustain) percussion instrument past pool size") {
    // A noise-channel drum/hi-hat instrument never receives NoteOff, and
    // relies entirely on its zero-sustain envelope finishing on its own to
    // free its voice slot back to the pool. Trigger it well more than
    // kMaxSynthVoices times, fully decaying each one first, and confirm
    // every trigger still gets a real voice rather than the pool silently
    // filling up and dropping notes.
    Mixer mixer;
    const uint32_t sampleRate = 1000;
    mixer.Configure(sampleRate);

    SynthInstrumentDef def;
    def.waveform = Waveform::Noise;
    def.envelope.attackSec = 0.001f;
    def.envelope.decaySec = 0.005f; // 5 samples @ 1000Hz
    def.envelope.sustainLevel = 0.0f;
    def.envelope.releaseSec = 0.001f;
    def.gain = 1.0f;
    mixer.AddSynthInstrument("hihat", def);

    const int triggerCount = static_cast<int>(Mixer::kMaxSynthVoices) * 3;
    for (int i = 0; i < triggerCount; ++i) {
        int handle = mixer.NoteOn("hihat", 4000.0f, 0.5f);
        CHECK(handle >= 0);

        // Render well past attack+decay so the envelope reaches Idle and
        // frees the voice before the next trigger.
        for (int s = 0; s < 20; ++s) {
            mixer.RenderNextSample();
        }
    }
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
