#include "engine/Mixer.h"

#include <algorithm>

namespace pmg {

namespace {

// Linear (not equal-power) pan law: at pan == 0.0 both gains are 1.0,
// exactly reproducing the old mono-summed loudness for any instrument that
// never sets pan. Real chip hardware (Game Boy NR51 hard L/R/both routing,
// SNES per-voice L/R volume registers) never did smooth psychoacoustic
// panning either, so a plain linear law is the period-plausible choice.
// pan is clamped to the documented [-1, 1] range first: outside it, either
// gain formula goes negative (phase-inverted and amplified beyond what
// "hard left/right" means), which a typo like "pan": 11 instead of 1.0
// would trigger silently otherwise.
void PanGains(float pan, float& leftGain, float& rightGain) {
    pan = std::clamp(pan, -1.0f, 1.0f);
    leftGain = pan <= 0.0f ? 1.0f : 1.0f - pan;
    rightGain = pan >= 0.0f ? 1.0f : 1.0f + pan;
}

} // namespace

void Mixer::Configure(uint32_t sampleRate) {
    m_sampleRate = sampleRate > 0 ? sampleRate : 48000;
}

void Mixer::Reset() {
    m_synthDefs.clear();
    m_sampleDefs.clear();
    m_mutedTracks.clear();
    m_trackGains.clear();
    for (auto& voice : m_voices) {
        voice.Reset();
    }
    for (auto& player : m_samplePlayers) {
        player.Reset();
    }
}

void Mixer::AddSynthInstrument(const InstrumentId& id, const SynthInstrumentDef& def) {
    m_synthDefs[id] = def;
}

void Mixer::AddSampleInstrument(const InstrumentId& id, const SampleInstrumentDef& def) {
    m_sampleDefs[id] = def;
}

bool Mixer::IsMuted(const InstrumentId& track) const {
    auto it = m_mutedTracks.find(track);
    return it != m_mutedTracks.end() && it->second;
}

float Mixer::TrackGain(const InstrumentId& track) const {
    auto it = m_trackGains.find(track);
    return it != m_trackGains.end() ? it->second : 1.0f;
}

int Mixer::NoteOn(const InstrumentId& instrument, float frequencyHz, float velocity, int gateDurationSamples) {
    if (IsMuted(instrument)) {
        return -1;
    }

    auto defIt = m_synthDefs.find(instrument);
    if (defIt == m_synthDefs.end()) {
        return -1;
    }

    for (size_t i = 0; i < kMaxSynthVoices; ++i) {
        if (!m_voices[i].IsActive()) {
            m_voices[i].Configure(m_sampleRate, defIt->second.waveform, defIt->second.dutyCycle,
                                   defIt->second.arpeggio, defIt->second.envelope, defIt->second.vibrato,
                                   defIt->second.fm);
            m_voices[i].NoteOn(frequencyHz, velocity, gateDurationSamples);
            m_voiceInstrument[i] = instrument;
            return static_cast<int>(i);
        }
    }
    return -1; // voice pool exhausted; note dropped
}

void Mixer::NoteOff(int voiceHandle) {
    if (voiceHandle < 0 || static_cast<size_t>(voiceHandle) >= kMaxSynthVoices) {
        return;
    }
    m_voices[static_cast<size_t>(voiceHandle)].NoteOff();
}

void Mixer::TriggerSample(const InstrumentId& instrument, float gainMultiplier) {
    if (IsMuted(instrument)) {
        return;
    }

    auto defIt = m_sampleDefs.find(instrument);
    if (defIt == m_sampleDefs.end() || !defIt->second.asset) {
        return;
    }

    for (auto& player : m_samplePlayers) {
        if (!player.IsActive()) {
            player.Trigger(defIt->second.asset, defIt->second.gain * gainMultiplier, defIt->second.pan);
            return;
        }
    }
    // sample player pool exhausted; trigger dropped
}

void Mixer::SetTrackMuted(const InstrumentId& track, bool muted) {
    m_mutedTracks[track] = muted;
}

void Mixer::SetTrackGain(const InstrumentId& track, float gain) {
    m_trackGains[track] = gain;
}

void Mixer::RenderNextStereoSample(float& left, float& right) {
    left = 0.0f;
    right = 0.0f;

    for (size_t i = 0; i < kMaxSynthVoices; ++i) {
        if (!m_voices[i].IsActive()) {
            continue;
        }
        const InstrumentId& instrument = m_voiceInstrument[i];
        auto defIt = m_synthDefs.find(instrument);
        float instrumentGain = defIt != m_synthDefs.end() ? defIt->second.gain : 1.0f;
        float pan = defIt != m_synthDefs.end() ? defIt->second.pan : 0.0f;

        float sample = m_voices[i].RenderSample() * instrumentGain * TrackGain(instrument);
        float leftGain, rightGain;
        PanGains(pan, leftGain, rightGain);
        left += sample * leftGain;
        right += sample * rightGain;
    }

    for (auto& player : m_samplePlayers) {
        if (!player.IsActive()) {
            continue;
        }
        float sample = player.RenderSample();
        float leftGain, rightGain;
        PanGains(player.GetPan(), leftGain, rightGain);
        left += sample * leftGain;
        right += sample * rightGain;
    }
}

float Mixer::RenderNextSample() {
    float left, right;
    RenderNextStereoSample(left, right);
    return (left + right) * 0.5f;
}

} // namespace pmg
