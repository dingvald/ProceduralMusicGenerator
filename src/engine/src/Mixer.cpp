#include "engine/Mixer.h"

namespace pmg {

void Mixer::Configure(uint32_t sampleRate) {
    m_sampleRate = sampleRate > 0 ? sampleRate : 48000;
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
            m_voices[i].Configure(m_sampleRate, defIt->second.waveform, defIt->second.dutyCycle, defIt->second.envelope);
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
            player.Trigger(defIt->second.asset, defIt->second.gain * gainMultiplier);
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

float Mixer::RenderNextSample() {
    float output = 0.0f;

    for (size_t i = 0; i < kMaxSynthVoices; ++i) {
        if (!m_voices[i].IsActive()) {
            continue;
        }
        const InstrumentId& instrument = m_voiceInstrument[i];
        auto defIt = m_synthDefs.find(instrument);
        float instrumentGain = defIt != m_synthDefs.end() ? defIt->second.gain : 1.0f;
        output += m_voices[i].RenderSample() * instrumentGain * TrackGain(instrument);
    }

    for (auto& player : m_samplePlayers) {
        if (!player.IsActive()) {
            continue;
        }
        output += player.RenderSample();
    }

    return output;
}

} // namespace pmg
