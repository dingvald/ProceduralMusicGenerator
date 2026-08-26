#include <cmath>
#include <memory>

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

TEST_CASE("Mixer pan defaults to center: RenderNextStereoSample matches RenderNextSample on both channels") {
    Mixer mixer;
    const uint32_t sampleRate = 1000;
    mixer.Configure(sampleRate);

    SynthInstrumentDef def;
    def.waveform = Waveform::Sine;
    def.envelope.attackSec = 0.0f;
    def.envelope.decaySec = 0.0f;
    def.envelope.sustainLevel = 1.0f;
    def.envelope.releaseSec = 0.005f;
    def.gain = 1.0f;
    mixer.AddSynthInstrument("centered", def);
    mixer.NoteOn("centered", 100.0f, 1.0f);

    float left, right;
    mixer.RenderNextStereoSample(left, right);
    CHECK(left == doctest::Approx(right));
}

TEST_CASE("Mixer hard-left pan silences the right channel") {
    Mixer mixer;
    const uint32_t sampleRate = 1000;
    mixer.Configure(sampleRate);

    SynthInstrumentDef def;
    def.waveform = Waveform::Sine;
    def.envelope.attackSec = 0.0f;
    def.envelope.decaySec = 0.0f;
    def.envelope.sustainLevel = 1.0f;
    def.envelope.releaseSec = 0.005f;
    def.gain = 1.0f;
    def.pan = -1.0f;
    mixer.AddSynthInstrument("panned_left", def);
    mixer.NoteOn("panned_left", 100.0f, 1.0f);

    bool heardLeft = false;
    for (int i = 0; i < 20; ++i) {
        float left, right;
        mixer.RenderNextStereoSample(left, right);
        if (std::fabs(left) > 0.01f) {
            heardLeft = true;
        }
        CHECK(std::fabs(right) < 1e-6f);
    }
    CHECK(heardLeft);
}

TEST_CASE("Mixer sums independently-panned instruments per channel") {
    Mixer mixer;
    const uint32_t sampleRate = 1000;
    mixer.Configure(sampleRate);

    SynthInstrumentDef left;
    left.waveform = Waveform::Sine;
    left.envelope.attackSec = 0.0f;
    left.envelope.decaySec = 0.0f;
    left.envelope.sustainLevel = 1.0f;
    left.envelope.releaseSec = 0.005f;
    left.pan = -1.0f;
    mixer.AddSynthInstrument("left", left);

    SynthInstrumentDef right;
    right.waveform = Waveform::Sine;
    right.envelope.attackSec = 0.0f;
    right.envelope.decaySec = 0.0f;
    right.envelope.sustainLevel = 1.0f;
    right.envelope.releaseSec = 0.005f;
    right.pan = 1.0f;
    mixer.AddSynthInstrument("right", right);

    mixer.NoteOn("left", 100.0f, 1.0f);
    mixer.NoteOn("right", 150.0f, 1.0f);

    bool heardBoth = false;
    for (int i = 0; i < 20; ++i) {
        float l, r;
        mixer.RenderNextStereoSample(l, r);
        if (std::fabs(l) > 0.01f && std::fabs(r) > 0.01f) {
            heardBoth = true;
        }
    }
    CHECK(heardBoth);
}

TEST_CASE("Mixer clamps a synth instrument's pan beyond +1.0/-1.0 instead of inverting phase") {
    Mixer mixer;
    const uint32_t sampleRate = 1000;
    mixer.Configure(sampleRate);

    // A "pan": 11.0 typo (meant to be 1.0) must behave exactly like
    // hard-right, not amplify/invert -- silent left channel, and the right
    // channel's gain must be exactly 1.0 (not 1.0 - 11.0 = -10.0).
    SynthInstrumentDef farRight;
    farRight.waveform = Waveform::Sine;
    farRight.envelope.attackSec = 0.0f;
    farRight.envelope.decaySec = 0.0f;
    farRight.envelope.sustainLevel = 1.0f;
    farRight.envelope.releaseSec = 0.005f;
    farRight.pan = 11.0f;
    mixer.AddSynthInstrument("far_right", farRight);
    mixer.NoteOn("far_right", 100.0f, 1.0f);

    SynthInstrumentDef farLeft;
    farLeft.waveform = Waveform::Sine;
    farLeft.envelope.attackSec = 0.0f;
    farLeft.envelope.decaySec = 0.0f;
    farLeft.envelope.sustainLevel = 1.0f;
    farLeft.envelope.releaseSec = 0.005f;
    farLeft.pan = -11.0f;
    mixer.AddSynthInstrument("far_left", farLeft);
    mixer.NoteOn("far_left", 150.0f, 1.0f);

    for (int i = 0; i < 20; ++i) {
        float left, right;
        mixer.RenderNextStereoSample(left, right);
        // Bounded by 1.0 (sine amplitude) * 1.0 (velocity) * 1.0 (gain) *
        // clamped pan gain of 1.0 -- an unclamped pan would let this exceed
        // 1.0 (e.g. up to 10x for pan == 11.0's raw 1.0 - 11.0 == -10.0 gain).
        CHECK(std::fabs(left) <= 1.0f + 1e-4f);
        CHECK(std::fabs(right) <= 1.0f + 1e-4f);
    }
}

TEST_CASE("Mixer clamps a sample instrument's pan the same way as a synth instrument's") {
    Mixer mixer;
    mixer.Configure(1000);

    auto asset = std::make_shared<SampleAsset>();
    asset->channels = 1;
    asset->sampleRate = 1000;
    asset->interleavedPCM.assign(20, 1.0f); // constant full-scale signal

    SampleInstrumentDef def;
    def.asset = asset;
    def.gain = 1.0f;
    def.pan = -11.0f; // typo'd hard-left
    mixer.AddSampleInstrument("far_left_sample", def);
    mixer.TriggerSample("far_left_sample");

    float left, right;
    mixer.RenderNextStereoSample(left, right);
    CHECK(std::fabs(left) <= 1.0f + 1e-4f);
    CHECK(std::fabs(right) < 1e-6f); // still fully silenced on the right, same as pan == -1.0
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
