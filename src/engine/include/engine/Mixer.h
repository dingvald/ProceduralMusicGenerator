#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include "engine/Arpeggiator.h"
#include "engine/Envelope.h"
#include "engine/Oscillator.h"
#include "engine/SampleAsset.h"
#include "engine/SamplePlayer.h"
#include "engine/SynthVoice.h"

namespace pmg {

using InstrumentId = std::string;

struct SynthInstrumentDef {
    Waveform waveform = Waveform::Sine;
    float dutyCycle = 0.5f; // only meaningful for Waveform::Square
    ArpeggioConfig arpeggio;
    ADSRParams envelope;
    float gain = 1.0f;
};

struct SampleInstrumentDef {
    std::shared_ptr<const SampleAsset> asset;
    float gain = 1.0f;
};

// Owns a fixed pool of SynthVoice/SamplePlayer slots and per-track
// mute/gain state. Exclusively owned and mutated by the audio thread (its
// setters are only ever called by AudioEngine's callback after draining
// ParameterBus), so Mixer itself needs no internal locking. This also makes
// it independently unit-testable without a real audio device.
class Mixer {
public:
    static constexpr size_t kMaxSynthVoices = 32;
    static constexpr size_t kMaxSamplePlayers = 8;

    void Configure(uint32_t sampleRate);

    void AddSynthInstrument(const InstrumentId& id, const SynthInstrumentDef& def);
    void AddSampleInstrument(const InstrumentId& id, const SampleInstrumentDef& def);

    // Returns a voice handle (>=0), or -1 if the instrument is unknown,
    // muted, or the voice pool is exhausted (note dropped). gateDurationSamples,
    // if >= 0, auto-releases the voice (as if NoteOff had been called) once
    // that many samples have been rendered; -1 (the default) holds the note
    // until an explicit NoteOff instead, matching pre-gate-support behavior.
    int NoteOn(const InstrumentId& instrument, float frequencyHz, float velocity, int gateDurationSamples = -1);
    void NoteOff(int voiceHandle);

    // No-op if the instrument is unknown, muted, or the sample-player pool
    // is exhausted (trigger dropped).
    void TriggerSample(const InstrumentId& instrument, float gainMultiplier = 1.0f);

    void SetTrackMuted(const InstrumentId& track, bool muted);
    void SetTrackGain(const InstrumentId& track, float gain);

    // Sums all active voices/sample players into one mono output sample.
    float RenderNextSample();

private:
    bool IsMuted(const InstrumentId& track) const;
    float TrackGain(const InstrumentId& track) const;

    uint32_t m_sampleRate = 48000;
    std::unordered_map<InstrumentId, SynthInstrumentDef> m_synthDefs;
    std::unordered_map<InstrumentId, SampleInstrumentDef> m_sampleDefs;
    std::array<SynthVoice, kMaxSynthVoices> m_voices;
    std::array<InstrumentId, kMaxSynthVoices> m_voiceInstrument;
    std::array<SamplePlayer, kMaxSamplePlayers> m_samplePlayers;
    std::unordered_map<InstrumentId, bool> m_mutedTracks;
    std::unordered_map<InstrumentId, float> m_trackGains;
};

} // namespace pmg
