#include <cmath>

#include "doctest/doctest.h"
#include "engine/Oscillator.h"
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

const ArpeggioConfig kNoArpeggio; // empty semitoneOffsets -> disabled
const VibratoConfig kNoVibrato;   // depthCents == 0 -> disabled
const FmConfig kNoFm;             // amount == 0 -> disabled

} // namespace

TEST_CASE("SynthVoice with no gate duration holds its note until an explicit NoteOff") {
    const uint32_t sampleRate = 1000;
    SynthVoice voice;
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, kNoArpeggio, MakeShortEnvelope(), kNoVibrato, kNoFm);
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
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, kNoArpeggio, MakeShortEnvelope(), kNoVibrato, kNoFm);
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
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, kNoArpeggio, MakeShortEnvelope(), kNoVibrato, kNoFm);
    voice.NoteOn(100.0f, 1.0f, 0);

    for (int i = 0; i < 20; ++i) {
        voice.RenderSample();
    }
    CHECK_FALSE(voice.IsActive());
}

TEST_CASE("SynthVoice with an arpeggio configured shifts frequency over time") {
    // 100 Hz at 1000 Hz sample rate is exactly 10 samples/period, and the
    // arp is set to advance every 10 samples too, so the voice's internal
    // oscillator phase lands back on exactly 0 right as the arp steps to
    // its second offset -- letting this compare directly against two
    // independent, freshly-phased reference oscillators instead of needing
    // to reason about phase continuity across the frequency change.
    const uint32_t sampleRate = 1000;

    ArpeggioConfig arp;
    arp.semitoneOffsets = {0, 12}; // root, then one octave up
    arp.rateHz = 100.0f;           // 10 samples/step

    ADSRParams flatEnvelope;
    flatEnvelope.attackSec = 0.0f;
    flatEnvelope.decaySec = 0.0f;
    flatEnvelope.sustainLevel = 1.0f; // envelope reaches a flat 1.0 gain almost immediately
    flatEnvelope.releaseSec = 0.01f;

    SynthVoice voice;
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, arp, flatEnvelope, kNoVibrato, kNoFm);
    voice.NoteOn(100.0f, 1.0f);

    Oscillator rootOsc;
    rootOsc.SetSampleRate(sampleRate);
    rootOsc.SetFrequency(100.0f);
    rootOsc.SetWaveform(Waveform::Sine);

    Oscillator octaveUpOsc;
    octaveUpOsc.SetSampleRate(sampleRate);
    octaveUpOsc.SetFrequency(200.0f);
    octaveUpOsc.SetWaveform(Waveform::Sine);

    for (int i = 0; i < 10; ++i) {
        CHECK(voice.RenderSample() == doctest::Approx(rootOsc.NextSample()).epsilon(0.001));
    }
    for (int i = 0; i < 10; ++i) {
        CHECK(voice.RenderSample() == doctest::Approx(octaveUpOsc.NextSample()).epsilon(0.001));
    }
}

TEST_CASE("SynthVoice with vibrato configured modulates frequency sinusoidally over time") {
    const uint32_t sampleRate = 1000;
    const float baseFrequency = 100.0f;
    const double kPi = 3.14159265358979323846;

    VibratoConfig vibrato;
    vibrato.rateHz = 50.0f;      // -> 20-sample LFO cycle @ 1000Hz
    vibrato.depthCents = 200.0f; // 2 semitones peak deviation

    ADSRParams flatEnvelope;
    flatEnvelope.attackSec = 0.0f;
    flatEnvelope.decaySec = 0.0f;
    flatEnvelope.sustainLevel = 1.0f;
    flatEnvelope.releaseSec = 0.01f;

    SynthVoice voice;
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, kNoArpeggio, flatEnvelope, vibrato, kNoFm);
    voice.NoteOn(baseFrequency, 1.0f);

    Oscillator reference;
    reference.SetSampleRate(sampleRate);
    reference.SetWaveform(Waveform::Sine);

    double vibratoPhase = 0.0;
    double vibratoIncrement = static_cast<double>(vibrato.rateHz) / sampleRate;
    for (int i = 0; i < 60; ++i) {
        double lfo = std::sin(2.0 * kPi * vibratoPhase);
        float semitones = vibrato.depthCents / 100.0f * static_cast<float>(lfo);
        float freqMultiplier = std::pow(2.0f, semitones / 12.0f);
        reference.SetFrequency(baseFrequency * freqMultiplier);

        CHECK(voice.RenderSample() == doctest::Approx(reference.NextSample()).epsilon(0.001));

        vibratoPhase += vibratoIncrement;
        if (vibratoPhase >= 1.0) {
            vibratoPhase -= std::floor(vibratoPhase);
        }
    }
}

TEST_CASE("SynthVoice with FM configured phase-modulates a sine carrier") {
    const uint32_t sampleRate = 1000;
    const float baseFrequency = 100.0f;

    FmConfig fm;
    fm.ratio = 2.0f;
    fm.amount = 0.5f;

    ADSRParams flatEnvelope;
    flatEnvelope.attackSec = 0.0f;
    flatEnvelope.decaySec = 0.0f;
    flatEnvelope.sustainLevel = 1.0f;
    flatEnvelope.releaseSec = 0.01f;

    SynthVoice voice;
    voice.Configure(sampleRate, Waveform::Sine, 0.5f, kNoArpeggio, flatEnvelope, kNoVibrato, fm);
    voice.NoteOn(baseFrequency, 1.0f);

    Oscillator refCarrier;
    refCarrier.SetSampleRate(sampleRate);
    refCarrier.SetWaveform(Waveform::Sine);
    refCarrier.SetFrequency(baseFrequency);

    Oscillator refModulator;
    refModulator.SetSampleRate(sampleRate);
    refModulator.SetWaveform(Waveform::Sine);
    refModulator.SetFrequency(baseFrequency * fm.ratio);

    for (int i = 0; i < 60; ++i) {
        float phaseModulation = refModulator.NextSample() * fm.amount;
        CHECK(voice.RenderSample() == doctest::Approx(refCarrier.NextSample(phaseModulation)).epsilon(0.001));
    }
}
