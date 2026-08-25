#include "doctest/doctest.h"
#include "engine/SynthVoice.h"

using namespace pmg;

namespace {

ADSRParams MakeShortEnvelope() {
    ADSRParams params;
    params.attackSec = 0.001f;
    params.decaySec = 0.001f;
    params.sustainLevel = 0.8f; // held indefinitely without a release
    params.releaseSec = 0.005f; // 5 samples @ 1000Hz
    return params;
}

} // namespace

TEST_CASE("SynthVoice with no gate duration holds its note until an explicit NoteOff") {
    const uint32_t sampleRate = 1000;
    SynthVoice voice;
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, MakeShortEnvelope());
    voice.NoteOn(100.0f, 1.0f); // gateDurationSamples defaults to -1: no auto-release

    for (int i = 0; i < 500; ++i) {
        voice.RenderSample();
    }
    CHECK(voice.IsActive()); // still held after many samples with no NoteOff

    voice.NoteOff();
    for (int i = 0; i < 20; ++i) {
        voice.RenderSample();
    }
    CHECK_FALSE(voice.IsActive());
}

TEST_CASE("SynthVoice auto-releases after gateDurationSamples without an explicit NoteOff") {
    const uint32_t sampleRate = 1000;
    const int gateDurationSamples = 10;

    SynthVoice voice;
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, MakeShortEnvelope());
    voice.NoteOn(100.0f, 1.0f, gateDurationSamples);

    for (int i = 0; i < gateDurationSamples; ++i) {
        voice.RenderSample();
    }
    CHECK(voice.IsActive()); // release has just begun, envelope hasn't reached Idle yet

    // releaseSec = 0.005s @ 1000Hz = 5 samples; give some headroom.
    for (int i = 0; i < 20; ++i) {
        voice.RenderSample();
    }
    CHECK_FALSE(voice.IsActive()); // released and finished on its own, with no NoteOff call
}

TEST_CASE("SynthVoice gateDurationSamples of 0 releases starting from the very first sample") {
    const uint32_t sampleRate = 1000;

    SynthVoice voice;
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, MakeShortEnvelope());
    voice.NoteOn(100.0f, 1.0f, 0);

    for (int i = 0; i < 20; ++i) {
        voice.RenderSample();
    }
    CHECK_FALSE(voice.IsActive());
}
